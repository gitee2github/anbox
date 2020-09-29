/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* this implements a GPS hardware library for the Android emulator.
 * the following code should be built as a shared library that will be
 * placed into /system/lib/hw/gps.goldfish.so
 *
 * it will be loaded by the code in hardware/libhardware/hardware.c
 * which is itself called from android_location_GpsLocationProvider.cpp
 */


#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define  LOG_TAG  "gps_mock"
//#include <cutils/log.h>
#include <log/log.h>
#include <cutils/sockets.h>
#include <hardware/gps.h>

#define GPS_DEBUG 0

#if GPS_DEBUG
#define DLOG(...)   ALOGD(__VA_ARGS__)
#else
#define DLOG(...)   ((void)0)
#endif

#define GPSFifoName     "/data/gps/fifo"

enum {
    CMD_QUIT = 0,
    CMD_START = 1,
    CMD_STOP = 2
};

#define  NMEA_MAX_SIZE  83

#define autoUpdateCountMax  10

typedef struct {
    int pos;
    int overflow;
    int utc_year;
    int utc_mon;
    int utc_day;
    int utc_diff;
    GpsLocation fix;
    gps_location_callback callback;
    gps_status_callback status_cb;
    gps_request_utc_time request_utc_time_cb;
    gps_sv_status_callback sv_status_cb;
    gps_nmea_callback nmea_cb;
    gps_xtra_download_request download_request_cb;
    agps_status_callback agps_status_cb;
    gps_ni_notify_callback notify_cb;
    agps_ril_request_set_id request_setid;
    agps_ril_request_ref_loc request_refloc;
    gps_geofence_transition_callback geofence_transition_callback;
    gps_geofence_status_callback geofence_status_callback;
    gps_measurement_callback measurement_callback;
    gnss_measurement_callback gnss_measurement_callback;
    char in[NMEA_MAX_SIZE + 1];
} NmeaReader;

typedef struct {
    int init;
    GpsCallbacks callbacks;
    GpsXtraCallbacks xtracallbacks;
    AGpsCallbacks agpscallbacks;
    GpsNiCallbacks nicallbacks;
    AGpsRilCallbacks rilcallbacks;
    GpsGeofenceCallbacks geocallbacks;
    GpsMeasurementCallbacks measurecallbacks;
    pthread_t thread1;
    pthread_t thread2;
    int started;
} GpsState;

typedef struct _GpsSvLink {
    char *gpsSvString;
    struct _GpsSvLink *next;
} GpsSvLink;

static char autoUpdate = 0;
static long system_time_diff = 0;
static GpsState _gps_state[1];
static NmeaReader pReader[1];
static GpsLocation gpsData = {sizeof(GpsLocation), 0, 30.495864, 114.535703, 51, 0, 30, 10, 0};
static GpsSvLink *pGpsSvInfo = NULL;
static char lastLocation[80] = {"longitude=114.535703:latitude=30.495864:altitude=51:utc=20170317102530"};
static const char *const GpsSvInfoInit[] = {
//PRN SNR ELEVATION AZIMUTH
        "01 33 45 140",
        "10 39 80 125",
        "12 17 30 062",
        "14 24 45 305",
        "18 36 40 136",
        "24 14 10 046",
        "25 31 40 106",
        "31 34 50 235",
        "32 30 60 340",
        NULL
};
static const char *const GpsStatusString[] = {
        "GPS_STATUS_NONE",
        "GPS_STATUS_SESSION_BEGIN",
        "GPS_STATUS_SESSION_END",
        "GPS_STATUS_ENGINE_ON",
        "GPS_STATUS_ENGINE_OFF",
        NULL
};
static const char *const AGpsTypeString[] = {
        "UNUSED",
        "AGPS_TYPE_SUPL",
        "AGPS_TYPE_C2K",
        NULL
};
static const char *const AGpsStatusString[] = {
        "UNUSED",
        "GPS_REQUEST_AGPS_DATA_CONN",
        "GPS_RELEASE_AGPS_DATA_CONN",
        "GPS_AGPS_DATA_CONNECTED",
        "GPS_AGPS_DATA_CONN_DONE",
        "GPS_AGPS_DATA_CONN_FAILED",
        NULL
};

static void gps_state_send(char command) {
    int len;
    int fifo_fd = -1;
    char cmd[20];
    switch (command) {
        case CMD_START:
            strcpy(cmd, "CMD_START");
            break;
        case CMD_STOP:
            strcpy(cmd, "CMD_STOP");
            break;
        case CMD_QUIT:
            strcpy(cmd, "CMD_QUIT");
            break;
        default:
            ALOGE("%s: unknow command = %d", __FUNCTION__, command);
            return;
    }
    DLOG("%s: command = %s", __FUNCTION__, cmd);
    if ((fifo_fd = open(GPSFifoName, O_WRONLY)) < 0) {
        ALOGE("open fifo \"%s\" (write) fail, error = %s\n", GPSFifoName, strerror(errno));
        return;
    }
    len = strlen(cmd);
    if (write(fifo_fd, cmd, len) != len) {
        ALOGE("%s: write \"%s\" to \"%s\" fail: %s", __FUNCTION__, cmd, GPSFifoName, strerror(errno));
    }
    close(fifo_fd);
}

static void freepGpsSvInfo() {
    GpsSvLink *tempLink;
    while (pGpsSvInfo != NULL) {
        tempLink = pGpsSvInfo;
        pGpsSvInfo = pGpsSvInfo->next;
        if (tempLink->gpsSvString != NULL) {
            free(tempLink->gpsSvString);
        }
        free(tempLink);
    }
}

static void gps_state_done(GpsState *s) {
    void *dummy;

    DLOG("%s, Enter", __FUNCTION__);
    if (s->thread1) {
        gps_state_send(CMD_QUIT);
        pthread_join(s->thread1, &dummy);
    }
    freepGpsSvInfo();
    s->init = 0;
    if (s->thread2) {
        pthread_join(s->thread2, &dummy);
    }
    DLOG("%s, Exit", __FUNCTION__);
}

static long getLocalTime(time_t *now) {
    struct tm tm_local;
    long local;

    localtime_r(now, &tm_local);

    local = tm_local.tm_sec +
            60 * (tm_local.tm_min +
                  60 * (tm_local.tm_hour +
                        24 * (tm_local.tm_yday +
                              365 * tm_local.tm_year)));
    return local;
}

static long getUTCTime(time_t *now) {
    struct tm tm_utc;
    long utc;

    gmtime_r(now, &tm_utc);

    utc = tm_utc.tm_sec +
          60 * (tm_utc.tm_min +
                60 * (tm_utc.tm_hour +
                      24 * (tm_utc.tm_yday +
                            365 * tm_utc.tm_year)));
    return utc;
}

static void nmea_reader_update_utc_diff(NmeaReader *r) {
    time_t now = time(NULL);
    r->utc_diff = getUTCTime(&now) - getLocalTime(&now);
}

static void nmea_reader_init(NmeaReader *r) {
    memset(r, 0, sizeof(*r));

    r->pos = 0;
    r->overflow = 0;
    r->utc_year = -1;
    r->utc_mon = -1;
    r->utc_day = -1;
    r->callback = NULL;
    r->status_cb = NULL;
    r->request_utc_time_cb = NULL;
    r->sv_status_cb = NULL;
    r->nmea_cb = NULL;
    r->download_request_cb = NULL;
    r->agps_status_cb = NULL;
    r->notify_cb = NULL;
    r->request_setid = NULL;
    r->request_refloc = NULL;
    r->geofence_transition_callback = NULL;
    r->geofence_status_callback = NULL;
    r->measurement_callback = NULL;
    r->gnss_measurement_callback = NULL;
    r->fix.size = sizeof(r->fix);

    nmea_reader_update_utc_diff(r);
}

static void nmea_reader_set_callback(NmeaReader *r, gps_location_callback cb) {
    r->callback = cb;
    if (cb != NULL && r->fix.flags != 0) {
        r->callback(&r->fix);
        r->fix.flags = 0;
    }
}

//定界符判断
static bool is_delimeter_ch(char ch) {
    return (' ' == ch) || ('\t' == ch) || (':' == ch);
}

static bool end_of_string(char ch) {
    return ('\0' == ch) || ('\r' == ch) || ('\n' == ch);
}

static char *get_str_begin(char *cmd_buf) {
    if (NULL == cmd_buf) {
        return NULL;
    }
    while (is_delimeter_ch(*cmd_buf)) {
        cmd_buf++;
    }
    if (end_of_string(*cmd_buf)) {
        return NULL;
    }
    return cmd_buf;
}

static char *get_str_end(char *cmd_buf) {
    if (NULL == cmd_buf) {
        return NULL;
    }
    while (!is_delimeter_ch(*cmd_buf) && !end_of_string(*cmd_buf)) {
        cmd_buf++;
    }
    return cmd_buf;
}

static int str2int(char *start, char *end) {
    int result = 0;
    char *p1 = start;
    char *p2 = end;

    if (p1 == NULL) {
        return -1;
    }
    while (*p1 < '0' || *p1 > '9') {
        p1++;
    }
    if (p2 == NULL || p2 <= p1) {
        p2 = p1;
        while (*p2 >= '0' && *p2 <= '9') {
            p2++;
        }
    }
    while (p1 < p2) {
        if (*p1 < '0' || *p1 > '9') {
            return -1;
        }
        result = result * 10 + *p1 - '0';
        p1++;
    }
    return result;
}

static float str2float(char *start, char *end) {
    float result = 0;
    char *p1 = start;
    char *p2 = end;
    char dotflag = 0;
    int dotfactor = 1;

    if (p1 == NULL) {
        return -1;
    }
    while (*p1 < '0' || *p1 > '9') {
        p1++;
    }
    if (p2 == NULL || p2 <= p1) {
        p2 = p1;
        while ((*p2 >= '0' && *p2 <= '9') || *p2 == '.') {
            p2++;
        }
    }
    while (p1 < p2) {
        if (*p1 == '.') {
            if (dotflag == 0) {
                dotflag = 1;
            } else {
                return -1;
            }
        } else {
            if (*p1 < '0' || *p1 > '9') {
                return -1;
            }
            result = result * 10 + *p1 - '0';
            if (dotflag == 1) {
                dotfactor = dotfactor * 10;
            }
        }
        p1++;
    }
    result = result / dotfactor;
    return result;
}

static bool getSvStatus(GpsSvStatus *status) {
    GpsSvLink *tempLink;
    char *p;
    int count = 0;

    if (pGpsSvInfo == NULL) {
        ALOGE("%s: pGpsSvInfo = NULL", __FUNCTION__);
        return false;
    }

    status->ephemeris_mask = 0ul;
    status->almanac_mask = 0ul;
    status->used_in_fix_mask = 0ul;

    tempLink = pGpsSvInfo;
    while (tempLink != NULL) {
        p = get_str_begin(tempLink->gpsSvString);
        status->sv_list[count].prn = str2int(p, NULL);
        p = get_str_end(p);
        p = get_str_begin(p);
        status->sv_list[count].snr = str2float(p, NULL);
        p = get_str_end(p);
        p = get_str_begin(p);
        status->sv_list[count].elevation = str2float(p, NULL);
        p = get_str_end(p);
        p = get_str_begin(p);
        status->sv_list[count].azimuth = str2float(p, NULL);
        if (status->sv_list[count].prn == -1 || status->sv_list[count].snr == -1 ||
            status->sv_list[count].elevation == -1 || status->sv_list[count].azimuth == -1) {
            ALOGE("%s: pGpsSvInfo format error: %s", __FUNCTION__, tempLink->gpsSvString);
        } else {
            DLOG("%s[%d]: prn = %d, snr = %.6f, elevation = %.6f, azimuth = %.6f", __FUNCTION__, count,
                 status->sv_list[count].prn, status->sv_list[count].snr, status->sv_list[count].elevation,
                 status->sv_list[count].azimuth);
            status->ephemeris_mask |= 1ul << (status->sv_list[count].prn - 1);
            status->almanac_mask |= 1ul << (status->sv_list[count].prn - 1);
            if (status->sv_list[count].snr >= 30) {
                status->used_in_fix_mask |= 1ul << (status->sv_list[count].prn - 1);
            }
            count++;
        }
        tempLink = tempLink->next;
    }
    status->size = sizeof(GpsSvStatus);
    status->num_svs = count;
    return true;
}

static bool strStartsWith(const char *buffer, const char *buff) {
    int i = 0;
    while (buff[i] != '\0') {
        if (buffer[i] != buff[i]) {
            return false;
        }
        i++;
    }
    return true;
}

static bool setGpsSvInfo(const char *const info[]) {
    int i = 0;
    GpsSvLink *lastLink = NULL;
    GpsSvLink *tempLink;

    freepGpsSvInfo();

    while (info[i] != NULL) {
        if ((tempLink = (GpsSvLink *) malloc(sizeof(GpsSvLink))) == NULL) {
            ALOGE("malloc tempLink fail, error = %s\n", strerror(errno));
            goto Fail3;
        }
        if ((tempLink->gpsSvString = (char *) malloc(strlen(info[i]) + 1)) == NULL) {
            ALOGE("malloc tempLink->gpsSvString fail, error = %s\n", strerror(errno));
	    if (tempLink != NULL) {
	        free(tempLink);
	    }
            goto Fail3;
        }
        strcpy(tempLink->gpsSvString, info[i]);
        tempLink->next = NULL;
        if (lastLink == NULL) {
            lastLink = tempLink;
            if (pGpsSvInfo == NULL) {
                pGpsSvInfo = tempLink;
            }
        } else {
            lastLink->next = tempLink;
            lastLink = tempLink;
        }
        i++;
    }
    return true;
    Fail3:
    freepGpsSvInfo();
    return false;
}

static void parse_gps_cmd(const char *const cmd, NmeaReader *reader) {
    char *p1 = NULL;
    char *p2 = NULL;
    char *pCommand = NULL;
    AGpsStatus astatus;
    char aGpsUpdate = 0;
    time_t now = time(NULL);
    char svUpdate = 0;

    asprintf(&pCommand, "%s", cmd);
    p1 = pCommand;

    DLOG("%s: %s", __FUNCTION__, p1);

    astatus.size = sizeof(AGpsStatus);

    gpsData.flags = 0;
    gpsData.accuracy = 10;
    gpsData.flags = GPS_LOCATION_HAS_ACCURACY;

    gpsData.timestamp = (long long) (now + system_time_diff) * 1000;

    while (p1 != NULL) {
        p2 = p1;
        if ((p1 = strchr(p1, ':')) != NULL) {
            *p1 = '\0';
            p1++;
        }
        DLOG("%s: parse arg -> %s", __FUNCTION__, p2);
        if (strStartsWith(p2, "longitude=")) {
            p2 += strlen("longitude=");
            gpsData.longitude = atof(p2);
            gpsData.flags |= GPS_LOCATION_HAS_LAT_LONG;
        } else if (strStartsWith(p2, "latitude=")) {
            p2 += strlen("latitude=");
            gpsData.latitude = atof(p2);
            gpsData.flags |= GPS_LOCATION_HAS_LAT_LONG;
        } else if (strStartsWith(p2, "altitude=")) {
            p2 += strlen("altitude=");
            gpsData.altitude = atof(p2);
            gpsData.flags |= GPS_LOCATION_HAS_ALTITUDE;
        } else if (strStartsWith(p2, "speed=")) {
            p2 += strlen("speed=");
            gpsData.speed = atof(p2);
            gpsData.flags |= GPS_LOCATION_HAS_SPEED;
        } else if (strStartsWith(p2, "bearing=")) {
            p2 += strlen("bearing=");
            gpsData.bearing = atof(p2);
            gpsData.flags |= GPS_LOCATION_HAS_BEARING;
        } else if (strStartsWith(p2, "utc=")) {
            struct tm tm;
            time_t fix_time;
            p2 += strlen("utc=");
            if (strlen(p2) >= 14) {
                tm.tm_year = str2int(p2, p2 + 4) - 1900;
                tm.tm_mon = str2int(p2 + 4, p2 + 6) - 1;
                tm.tm_mday = str2int(p2 + 6, p2 + 8);
                tm.tm_hour = str2int(p2 + 8, p2 + 10);
                tm.tm_min = str2int(p2 + 10, p2 + 12);
                tm.tm_sec = str2int(p2 + 12, p2 + 14);
                tm.tm_isdst = -1;
                fix_time = mktime(&tm);
                system_time_diff = fix_time - now;
                gpsData.timestamp = (long long) fix_time * 1000;
                gpsData.flags |= GPS_LOCATION_HAS_LAT_LONG;
            } else {
                ALOGE("%s: utc format error = %s", __FUNCTION__, p2);
            }
        } else if (strStartsWith(p2, "status=")) {
            GpsStatus status;
            int i = 0;
            status.size = sizeof(GpsStatus);
            p2 += strlen("status=");
            while (GpsStatusString[i] != NULL) {
                if (!strcmp(p2, GpsStatusString[i])) {
                    break;
                }
                i++;
            }
            if (GpsStatusString[i] != NULL) {
                status.status = i;
                if (reader != NULL && reader->status_cb != NULL) {
                    DLOG("%s: report status = %s", __FUNCTION__, GpsStatusString[i]);
                    reader->status_cb(&status);
                } else {
                    ALOGE("%s: reader or reader->status_cb is NULL", __FUNCTION__);
                }
            } else {
                ALOGE("%s: unknow status = %s", __FUNCTION__, p2);
            }
        } else if (strStartsWith(p2, "ntptime")) {
            if (reader != NULL && reader->request_utc_time_cb != NULL) {
                DLOG("request ntp time");
                reader->request_utc_time_cb();
            } else {
                ALOGE("%s: reader or reader->request_utc_time_cb is NULL", __FUNCTION__);
            }
        } else if (strStartsWith(p2, "satellite=")) {
            char *svInfo[GPS_MAX_SVS];
            char *p3;
            int i = 0;
            p2 += strlen("satellite=");
            if (strlen(p2) >= 7) {        //一组卫星数据至少7个字符
                p3 = p2;
                while (*p3 != '\0') {
                    if (*p3 == '-') {
                        *p3 = ' ';
                    } else if (*p3 == '/') {
                        *p3 = '|';
                    }
                    p3++;
                }
                while ((p2 != NULL) && (i < GPS_MAX_SVS)) {
                    p3 = p2;
                    if ((p2 = strchr(p2, '|')) != NULL) {
                        *p2 = '\0';
                        p2++;
                    }
                    svInfo[i++] = p3;
                }
            }
            svInfo[i] = NULL;
            if (setGpsSvInfo((const char *const *) svInfo)) {
                svUpdate = 1;
            }
        } else if (strStartsWith(p2, "nmea=")) {
            p2 += strlen("nmea=");
            if (reader != NULL && reader->nmea_cb != NULL) {
                reader->nmea_cb(gpsData.timestamp, p2, strlen(p2));
            } else {
                ALOGE("%s: reader or reader->nmea_cb is NULL", __FUNCTION__);
            }
        } else if (strStartsWith(p2, "downloadxtra")) {
            if (reader != NULL && reader->download_request_cb != NULL) {
                DLOG("download xtra gps data");
                reader->download_request_cb();
            } else {
                ALOGE("%s: reader or reader->download_request_cb is NULL", __FUNCTION__);
            }
        } else if (strStartsWith(p2, "AGpsType=")) {
            int i = 0;
            p2 += strlen("AGpsType=");
            while (AGpsTypeString[i] != NULL) {
                if (!strcmp(p2, AGpsTypeString[i])) {
                    break;
                }
                i++;
            }
            if (AGpsTypeString[i] != NULL) {
                astatus.type = i;
                aGpsUpdate++;
            } else {
                ALOGE("%s: unknow astatus type = %s", __FUNCTION__, p2);
            }
        } else if (strStartsWith(p2, "AGpsStatus=")) {
            int i = 0;
            p2 += strlen("AGpsStatus=");
            while (AGpsStatusString[i] != NULL) {
                if (!strcmp(p2, AGpsStatusString[i])) {
                    break;
                }
                i++;
            }
            if (AGpsStatusString[i] != NULL) {
                astatus.status = i;
                aGpsUpdate++;
            } else {
                ALOGE("%s: unknow astatus = %s", __FUNCTION__, p2);
            }
        } else if (strStartsWith(p2, "AGpsSinAddr=")) {
            struct sockaddr_in *addr_v4 = (struct sockaddr_in *) &astatus.addr;
            p2 += strlen("AGpsSinAddr=");
            astatus.ipaddr = INADDR_NONE;
            addr_v4->sin_family = AF_INET;
            addr_v4->sin_port = 1234;
            inet_aton(p2, &(addr_v4->sin_addr));
            astatus.addr.ss_family = AF_INET;
            if (aGpsUpdate >= 2) {
                if (reader != NULL && reader->agps_status_cb != NULL) {
                    reader->agps_status_cb(&astatus);
                } else {
                    ALOGE("%s: reader or reader->agps_status_cb is NULL", __FUNCTION__);
                }
            } else {
                DLOG("%s: need to set \"AGpsType\" and \"AGpsStatus\" before \"AGpsSinAddr\"", __FUNCTION__);
            }
        } else if (strStartsWith(p2, "NiNotification=")) {
            GpsNiNotification ni;
            p2 += strlen("NiNotification=");
            ni.size = sizeof(GpsNiNotification);
            ni.notification_id = str2int(p2, NULL);
            p2 = get_str_end(p2);
            p2 = get_str_begin(p2);
            ni.notify_flags = str2int(p2, NULL);
            ni.timeout = 0;
            ni.default_response = GPS_NI_RESPONSE_ACCEPT;
            if (reader != NULL && reader->notify_cb != NULL) {
                reader->notify_cb(&ni);
            } else {
                ALOGE("%s: reader or reader->notify_cb is NULL", __FUNCTION__);
            }
        } else if (strStartsWith(p2, "agpsrilid=")) {
            uint32_t flags;
            p2 += strlen("agpsrilid=");
            flags = str2int(p2, NULL);
            if (reader != NULL && reader->request_setid != NULL) {
                reader->request_setid(flags);
            } else {
                ALOGE("%s: reader or reader->request_setid is NULL", __FUNCTION__);
            }
        } else if (strStartsWith(p2, "agpsrilloc=")) {
            uint32_t flags;
            p2 += strlen("agpsrilloc=");
            flags = str2int(p2, NULL);
            if (reader != NULL && reader->request_refloc != NULL) {
                reader->request_refloc(flags);
            } else {
                ALOGE("%s: reader or reader->request_refloc is NULL", __FUNCTION__);
            }
        } else if (strStartsWith(p2, "geofence_transition=")) {
            int32_t geofence_id;
            int32_t transition;
            p2 += strlen("geofence_transition=");
            geofence_id = str2int(p2, NULL);
            p2 = get_str_end(p2);
            p2 = get_str_begin(p2);
            transition = str2int(p2, NULL);
            if (reader != NULL && reader->geofence_transition_callback != NULL) {
                reader->geofence_transition_callback(geofence_id, &gpsData, transition, gpsData.timestamp);
            } else {
                ALOGE("%s: reader or reader->geofence_transition_callback is NULL", __FUNCTION__);
            }
        } else if (strStartsWith(p2, "geofence_status=")) {
            int32_t status;
            p2 += strlen("geofence_status=");
            status = str2int(p2, NULL);
            if (reader != NULL && reader->geofence_status_callback != NULL) {
                reader->geofence_status_callback(status, &gpsData);
            } else {
                ALOGE("%s: reader or reader->geofence_status_callback is NULL", __FUNCTION__);
            }
        } else {
            ALOGE("%s: unknow command = %s", __FUNCTION__, p2);
        }
    }
    if (svUpdate == 1 || (gpsData.flags &
                          (GPS_LOCATION_HAS_LAT_LONG | GPS_LOCATION_HAS_ALTITUDE | GPS_LOCATION_HAS_SPEED |
                           GPS_LOCATION_HAS_BEARING))) {
        memset((void *) lastLocation, '\0', sizeof(lastLocation));
        sprintf(lastLocation, "longitude=%.6f:latitude=%.6f:altitude=%.2f", gpsData.longitude, gpsData.latitude,
                gpsData.altitude);
        if (reader != NULL && reader->callback != NULL && reader->sv_status_cb != NULL) {
            GpsSvStatus sv;
            if (getSvStatus(&sv)) {
                DLOG("report satellite status.");
                reader->sv_status_cb(&sv);
                DLOG("%s: report  longitude = %.6f, latitude = %.6f, altitude = %.6f, speed = %.2f, timestamp = %ld",
                     __FUNCTION__, gpsData.longitude, gpsData.latitude, gpsData.altitude, gpsData.speed,
                     gpsData.timestamp);
                reader->callback(&gpsData);
                if (autoUpdate < autoUpdateCountMax) {
                    autoUpdate = 0;
                }
            }
        } else {
            ALOGE("%s: reader or reader->callback or reader->sv_status_cb is NULL", __FUNCTION__);
        }
    }
    if (pCommand != NULL) {
        free(pCommand);
    }
}

static void gps_state_thread(void *arg) {
    GpsState *state = (GpsState *) arg;
    int fifo_fd = -1;
    int len;
    char buffer[500];

    nmea_reader_init(pReader);

    DLOG("gps thread running\n");
    for (;;) {
        //阻塞式读
        if ((fifo_fd = open(GPSFifoName, O_RDONLY)) < 0) {
            ALOGE("open fifo \"%s\"(read) fail, error = %s\n", GPSFifoName, strerror(errno));
            return;
        }
        buffer[0] = '\0';
        do {
            len = read(fifo_fd, buffer, sizeof(buffer));
            if ((len > 0) && (len < (int) sizeof(buffer))) {
                buffer[len] = '\0';
                break;
            }
        } while (len > 0);
        close(fifo_fd);
        while (buffer[len - 1] == '\n' || buffer[len - 1] == '\r') {
            buffer[len - 1] = '\0';
            len--;
        }
        DLOG("gps thread received event: %s", buffer);
        if (!strcmp(buffer, "CMD_QUIT")) {
            DLOG("gps thread quitting on demand");
            return;
        } else if (!strcmp(buffer, "CMD_START")) {
            if (!state->started) {
                DLOG("gps thread starting location_cb=%p", state->callbacks.location_cb);
                state->started = 1;
                nmea_reader_set_callback(pReader, state->callbacks.location_cb);
                pReader->status_cb = state->callbacks.status_cb;
                pReader->request_utc_time_cb = state->callbacks.request_utc_time_cb;
                pReader->sv_status_cb = state->callbacks.sv_status_cb;
                pReader->nmea_cb = state->callbacks.nmea_cb;
                pReader->download_request_cb = state->xtracallbacks.download_request_cb;
                pReader->agps_status_cb = state->agpscallbacks.status_cb;
                pReader->notify_cb = state->nicallbacks.notify_cb;
                pReader->request_setid = state->rilcallbacks.request_setid;
                pReader->request_refloc = state->rilcallbacks.request_refloc;
                pReader->geofence_transition_callback = state->geocallbacks.geofence_transition_callback;
                pReader->geofence_status_callback = state->geocallbacks.geofence_status_callback;
                pReader->measurement_callback = state->measurecallbacks.measurement_callback;
                pReader->gnss_measurement_callback = state->measurecallbacks.gnss_measurement_callback;
                parse_gps_cmd("status=GPS_STATUS_ENGINE_ON", pReader);
                parse_gps_cmd((const char *const) lastLocation, pReader);
                autoUpdate = autoUpdateCountMax + 1;
            }
        } else if (!strcmp(buffer, "CMD_STOP")) {
            if (state->started) {
                DLOG("gps thread stopping");
                parse_gps_cmd("status=GPS_STATUS_ENGINE_OFF", pReader);
                state->started = 0;
                nmea_reader_set_callback(pReader, NULL);
                pReader->status_cb = NULL;
                pReader->request_utc_time_cb = NULL;
                pReader->sv_status_cb = NULL;
                pReader->nmea_cb = NULL;
                pReader->download_request_cb = NULL;
                pReader->agps_status_cb = NULL;
                pReader->notify_cb = NULL;
                pReader->request_setid = NULL;
                pReader->request_refloc = NULL;
                pReader->geofence_transition_callback = NULL;
                pReader->geofence_status_callback = NULL;
                pReader->measurement_callback = NULL;
                pReader->gnss_measurement_callback = NULL;
            }
        } else {
            parse_gps_cmd((const char *const) buffer, pReader);
            autoUpdate = autoUpdateCountMax + 1;
        }
    }
}

static void gps_report_thread(void *arg) {
    GpsState *state = (GpsState *) arg;
    for (;;) {
        sleep(1);
        DLOG("%s: wakeup", __FUNCTION__);
        if (state->started) {
            if (autoUpdate >= autoUpdateCountMax) {
                DLOG("%s: report lastLocation", __FUNCTION__);
                parse_gps_cmd((const char *const) lastLocation, pReader);
                if (autoUpdate > autoUpdateCountMax) {
                    autoUpdate--;
                } else {
                    autoUpdate = 0;
                }
            } else {
                autoUpdate++;
            }
        }
        if (!state->init) {
            DLOG("%s: Exit", __FUNCTION__);
            return;
        }
    }
}

static void gps_state_init(GpsState *state, GpsCallbacks *callbacks) {
    state->init = 1;
    state->started = 0;

    DLOG("%s", __FUNCTION__);
    if (access(GPSFifoName, F_OK) < 0) {
        DLOG("%s, mkfifo %s", __FUNCTION__, GPSFifoName);
        if (mkfifo(GPSFifoName, 0666) != 0) {
            ALOGE("could not create fifo \"%s\", error = %s\n", GPSFifoName, strerror(errno));
        }
    }
    //初始化搜星信息
    if (setGpsSvInfo(GpsSvInfoInit) == false) {
        goto Fail1;
    }
    //创建GPS HAL主线程
    state->thread1 = callbacks->create_thread_cb("gps_state_thread", gps_state_thread, state);
    if (!state->thread1) {
        ALOGE("could not create gps thread1: %s", strerror(errno));
        goto Fail1;
    }
    state->callbacks = *callbacks;
    state->thread2 = callbacks->create_thread_cb("gps_report_thread", gps_report_thread, state);
    if (!state->thread2) {
        ALOGE("could not create gps thread2: %s", strerror(errno));
        goto Fail1;
    }
    DLOG("gps state initialized");
    return;
    Fail1:
    gps_state_done(state);
}

static int mock_gps_init(GpsCallbacks *callbacks) {
    GpsState *s = _gps_state;
    DLOG("%s", __FUNCTION__);
    if (!s->init) {
        gps_state_init(s, callbacks);
    }

    return 0;
}

static int mock_gps_start() {
    GpsState *s = _gps_state;
    DLOG("%s", __FUNCTION__);
    if (!s->init) {
        ALOGE("%s: called with uninitialized state !!", __FUNCTION__);
        return -1;
    }
    gps_state_send(CMD_START);
    return 0;
}

static int mock_gps_stop() {
    GpsState *s = _gps_state;
    DLOG("%s", __FUNCTION__);
    if (!s->init) {
        ALOGE("%s: called with uninitialized state !!", __FUNCTION__);
        return -1;
    }
    gps_state_send(CMD_STOP);
    return 0;
}

static void mock_gps_cleanup(void) {
    GpsState *s = _gps_state;
    DLOG("%s, Enter", __FUNCTION__);
    if (s->init) {
        gps_state_done(s);
    }
    DLOG("%s, Exit", __FUNCTION__);
}

static int mock_gps_inject_time(GpsUtcTime time __unused, int64_t timeReference __unused, int uncertainty __unused) {
    DLOG("%s: time = %ld, timeReference = %ld, uncertainty = %d", __FUNCTION__, time, timeReference, uncertainty);
    return 0;
}

static int mock_gps_inject_location(double latitude __unused, double longitude __unused, float accuracy __unused) {
    DLOG("%s: latitude = %.6f, longitude = %.6f, accuracy = %.3f", __FUNCTION__, latitude, longitude, accuracy);
    return 0;
}

static void mock_gps_delete_aiding_data(GpsAidingData flags __unused) {
    DLOG("%s: flags = %d", __FUNCTION__, flags);
}

static int mock_gps_set_position_mode(GpsPositionMode mode __unused, GpsPositionRecurrence recurrence __unused,
                                      uint32_t min_interval __unused, uint32_t preferred_accuracy __unused,
                                      uint32_t preferred_time __unused) {
    DLOG("%s: mode = %d, recurrence = %d, min_interval = %d, preferred_accuracy = %d, preferred_time = %d",
         __FUNCTION__, mode, recurrence, min_interval, preferred_accuracy, preferred_time);
    return 0;
}

static int proxy_gps_xtra_init(GpsXtraCallbacks *callbacks) {
    DLOG("%s", __FUNCTION__);
    _gps_state->xtracallbacks = *callbacks;
    return 0;
}

static int proxy_gps_xtra_inject_xtra_data(char *data __unused, int length __unused) {
    DLOG("%s: data = %s, length = %d", __FUNCTION__, data, length);
    return 0;
}

static GpsXtraInterface s_ProxyGpsXtraInterface = {
        sizeof(GpsXtraInterface),
        proxy_gps_xtra_init,
        proxy_gps_xtra_inject_xtra_data
};

static void proxy_agps_init(AGpsCallbacks *callbacks) {
    DLOG("%s", __FUNCTION__);
    _gps_state->agpscallbacks = *callbacks;
}

static int proxy_agps_data_conn_open(const char *apn __unused) {
    DLOG("%s: apn = %s", __FUNCTION__, apn);
    return 0;
}

static int proxy_agps_data_conn_closed() {
    DLOG("%s", __FUNCTION__);
    return 0;
}

static int proxy_agps_data_conn_failed() {
    DLOG("%s", __FUNCTION__);
    return 0;
}

static int proxy_agps_set_server(AGpsType type __unused, const char *hostname __unused, int port __unused) {
    DLOG("%s: type = %d, hostname = %s, port = %d", __FUNCTION__, type, hostname, port);
    return 0;
}

static int proxy_agps_data_conn_open_with_apn_ip_type(const char *apn __unused, ApnIpType apnIpType __unused) {
    DLOG("%s: apn = %s, apnIpType = %d", __FUNCTION__, apn, apnIpType);
    return 0;
}

static AGpsInterface s_ProxyAGpsInterface = {
        sizeof(AGpsInterface),
        proxy_agps_init,
        proxy_agps_data_conn_open,
        proxy_agps_data_conn_closed,
        proxy_agps_data_conn_failed,
        proxy_agps_set_server,
        proxy_agps_data_conn_open_with_apn_ip_type
};

static void proxy_gps_ni_init(GpsNiCallbacks *callbacks) {
    DLOG("%s", __FUNCTION__);
    _gps_state->nicallbacks = *callbacks;
}

static void proxy_gps_ni_respond(int notif_id __unused, GpsUserResponseType user_response __unused) {
    DLOG("%s: notif_id = %d, user_response = %d", __FUNCTION__, notif_id, user_response);
}

static GpsNiInterface s_ProxyGpsNiInterface = {
        sizeof(GpsNiInterface),
        proxy_gps_ni_init,
        proxy_gps_ni_respond
};

static void proxy_agps_ril_init(AGpsRilCallbacks *callbacks) {
    DLOG("%s", __FUNCTION__);
    _gps_state->rilcallbacks = *callbacks;
}

static void
proxy_agps_ril_set_ref_location(const AGpsRefLocation *agps_reflocation __unused, size_t sz_struct __unused) {
    DLOG("%s: type = %d, sz_struct = %zu", __FUNCTION__, agps_reflocation->type, sz_struct);
}

static void proxy_agps_ril_set_set_id(AGpsSetIDType type __unused, const char *setid __unused) {
    DLOG("%s: type = %d, setid = %s", __FUNCTION__, type, setid);
}

static void proxy_agps_ril_ni_message(uint8_t *msg __unused, size_t len __unused) {
    DLOG("%s: msg = %d, len = %zu", __FUNCTION__, msg[0], len);
}

static void proxy_agps_ril_update_network_state(int connected __unused, int type __unused, int roaming __unused,
                                                const char *extra_info __unused) {
    DLOG("%s: connected = %d, type = %d, roaming = %d, extra_info = %s", __FUNCTION__, connected, type, roaming,
         extra_info);
}

static void proxy_agps_ril_update_network_availability(int avaiable __unused, const char *apn __unused) {
    DLOG("%s: avaiable = %d, apn = %s", __FUNCTION__, avaiable, apn);
}

static AGpsRilInterface s_ProxyAGpsRilInterface = {
        sizeof(AGpsRilInterface),
        proxy_agps_ril_init,
        proxy_agps_ril_set_ref_location,
        proxy_agps_ril_set_set_id,
        proxy_agps_ril_ni_message,
        proxy_agps_ril_update_network_state,
        proxy_agps_ril_update_network_availability
};

void proxy_gps_geofencing_init(GpsGeofenceCallbacks *callbacks) {
    DLOG("%s", __FUNCTION__);
    _gps_state->geocallbacks = *callbacks;
}

void proxy_gps_geofencing_add_geofence_area(int32_t geofence_id __unused, double latitude __unused, double longitude
                                            __unused, double radius_meters __unused, int last_transition __unused,
                                            int monitor_transitions __unused, int notification_responsiveness_ms
                                            __unused, int unknown_timer_ms __unused) {
    DLOG("%s: id=%d, latitude=%.6f, longitude=%.6f, radius_meters=%.6f, last_transition = %d, monitor_transitions = %d, notification_responsiveness_ms = %d, unknown_timer_ms = %d",
         __FUNCTION__, geofence_id, latitude, longitude, radius_meters, last_transition, monitor_transitions,
         notification_responsiveness_ms, unknown_timer_ms);
}

void proxy_gps_geofencing_pause_geofence(int32_t geofence_id __unused) {
    DLOG("%s: geofence_id = %d", __FUNCTION__, geofence_id);
}

void proxy_gps_geofencing_resume_geofence(int32_t geofence_id __unused, int monitor_transitions __unused) {
    DLOG("%s: geofence_id = %d, monitor_transitions = %d", __FUNCTION__, geofence_id, monitor_transitions);
}

void proxy_gps_geofencing_remove_geofence_area(int32_t geofence_id __unused) {
    DLOG("%s: geofence_id = %d", __FUNCTION__, geofence_id);
}

static GpsGeofencingInterface s_ProxyGpsGeofencingInterface = {
        sizeof(GpsGeofencingInterface),
        proxy_gps_geofencing_init,
        proxy_gps_geofencing_add_geofence_area,
        proxy_gps_geofencing_pause_geofence,
        proxy_gps_geofencing_resume_geofence,
        proxy_gps_geofencing_remove_geofence_area
};

int proxy_gps_measurement_init(GpsMeasurementCallbacks *callbacks) {
    DLOG("%s", __FUNCTION__);
    _gps_state->measurecallbacks = *callbacks;
    return GPS_MEASUREMENT_OPERATION_SUCCESS;
}

void proxy_gps_measurement_close() {
    DLOG("%s", __FUNCTION__);
}

static GpsMeasurementInterface s_ProxyGpsMeasurementInterface = {
        sizeof(GpsMeasurementInterface),
        proxy_gps_measurement_init,
        proxy_gps_measurement_close
};

static const void *mock_gps_get_extension(const char *name) {
    DLOG("%s: name = %s", __FUNCTION__, name);
    if (!strcmp(name, GPS_XTRA_INTERFACE)) {
        return &s_ProxyGpsXtraInterface;
    } else if (!strcmp(name, AGPS_INTERFACE)) {
        return &s_ProxyAGpsInterface;
    } else if (!strcmp(name, GPS_NI_INTERFACE)) {
        return &s_ProxyGpsNiInterface;
    } else if (!strcmp(name, AGPS_RIL_INTERFACE)) {
        return &s_ProxyAGpsRilInterface;
    } else if (!strcmp(name, GPS_GEOFENCING_INTERFACE)) {
        return &s_ProxyGpsGeofencingInterface;
    } else if (!strcmp(name, GPS_MEASUREMENT_INTERFACE)) {
        return &s_ProxyGpsMeasurementInterface;
    }
    return NULL;
}

static const GpsInterface mockGpsInterface = {
        sizeof(GpsInterface),
        mock_gps_init,
        mock_gps_start,
        mock_gps_stop,
        mock_gps_cleanup,
        mock_gps_inject_time,
        mock_gps_inject_location,
        mock_gps_delete_aiding_data,
        mock_gps_set_position_mode,
        mock_gps_get_extension,
};

const GpsInterface *gps__get_gps_interface(struct gps_device_t *dev __unused) {
    DLOG("%s: dev = %p", __FUNCTION__, dev);
    return &mockGpsInterface;
}

static int open_gps(const struct hw_module_t *module, char const *name __unused, struct hw_device_t **device) {
    struct gps_device_t *dev = (struct gps_device_t *) malloc(sizeof(struct gps_device_t));
    DLOG("%s: name = %s", __FUNCTION__, name);
    memset((void *) dev, 0, sizeof(*dev));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t *) module;
//  dev->common.close = (int (*)(struct hw_device_t*))close_lights;
    dev->get_gps_interface = gps__get_gps_interface;

    *device = (struct hw_device_t *) dev;
    return 0;
}

static struct hw_module_methods_t gps_module_methods = {
        .open = open_gps
};

struct hw_module_t HAL_MODULE_INFO_SYM = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = GPS_HARDWARE_MODULE_ID,
        .name = "Goldfish GPS Module",
        .author = "The Android Open Source Project",
        .methods = &gps_module_methods,
};
