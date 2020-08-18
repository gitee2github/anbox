/*
 * Copyright (C) 2016 Simon Fels <morphis@gravedo.de>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "anbox/daemon.h"
#include "anbox/utils.h"
#include <malloc.h>

int main(int argc, char **argv) {
  /*  Note: Nowadays, glibc uses a dynamic mmap threshold by
      default.  The initial value of the threshold is 128*1024, but
      when blocks larger than the current threshold and less than or
      equal to DEFAULT_MMAP_THRESHOLD_MAX are freed, the threshold
      is adjusted upward to the size of the freed block.  When
      dynamic mmap thresholding is in effect, the threshold for
      trimming the heap is also dynamically adjusted to be twice the
      dynamic mmap threshold.  Dynamic adjustment of the mmap
      threshold is disabled if any of the M_TRIM_THRESHOLD,
      M_TOP_PAD, M_MMAP_THRESHOLD, or M_MMAP_MAX parameters is set. */

  constexpr int KILO_BYTES = 1024;
  constexpr int DEFAULT_THRESHOLD = 128 * KILO_BYTES;
  constexpr int DEFAULT_TOP_PAD = 128 * KILO_BYTES;
  constexpr int DEFAULT_MMAP_MAX = 64 * KILO_BYTES;
  mallopt(M_ARENA_MAX, 2);
  mallopt(M_MMAP_THRESHOLD, DEFAULT_THRESHOLD);
  mallopt(M_TRIM_THRESHOLD, DEFAULT_THRESHOLD);
  mallopt(M_TOP_PAD, DEFAULT_TOP_PAD);
  mallopt(M_MMAP_MAX, DEFAULT_MMAP_MAX);
  anbox::Daemon daemon;
  return daemon.Run(anbox::utils::collect_arguments(argc, argv));
}
