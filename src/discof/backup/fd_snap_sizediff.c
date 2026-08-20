#include "fd_backup.h"
#include "../../flamenco/accdb/fd_accdb.h"
#include <zstd.h>
#include <stdlib.h>
#include <string.h>

FD_IMPORT_BINARY( testfile, "src/discof/backup/agave_50mb.raw" );

typedef struct {
  ulong                  hash;
  snap_acc_hdr_t const * hdr;
  ulong                  rec_sz;
} parsed_acc_t;

static int
acc_hash_cmp( void const * _a,
              void const * _b ) {
  parsed_acc_t const * a = (parsed_acc_t const *)_a;
  parsed_acc_t const * b = (parsed_acc_t const *)_b;
  if( a->hash < b->hash ) return -1;
  if( a->hash > b->hash ) return  1;
  return 0;
}

int
main( int     argc,
      char ** argv ) {
  fd_boot( &argc, &argv );

  FD_LOG_NOTICE(( "Loaded %lu bytes from embedded testfile at %p", testfile_sz, (void const *)testfile ));

  ulong max_accs = 200000UL;
  parsed_acc_t * accs = (parsed_acc_t *)malloc( sizeof(parsed_acc_t) * max_accs );
  FD_TEST( accs );

  uchar const * cur = testfile;
  uchar const * end = testfile + testfile_sz;
  ulong acc_cnt     = 0UL;
  ulong total_raw   = 0UL;

  while( cur < end ) {
    /* Skip TAR headers */
    if( cur + 512UL <= end ) {
      if( 0==memcmp( cur, "accounts/", 9UL ) || 0==memcmp( cur + 257UL, "ustar", 5UL ) ) {
        cur += 512UL;
        continue;
      }
    }

    if( cur + sizeof(snap_acc_hdr_t) <= end ) {
      snap_acc_hdr_t const * hdr = (snap_acc_hdr_t const *)cur;

      if( hdr->slot==0UL && hdr->data_len==0UL ) {
        int all_zeros = 1;
        for( ulong i=0UL; i<sizeof(snap_acc_hdr_t); i++ ) {
          if( cur[i] != 0 ) { all_zeros = 0; break; }
        }
        if( all_zeros ) {
          ulong rem = (ulong)(cur - testfile) % 512UL;
          cur += rem ? (512UL - rem) : 512UL;
          continue;
        }
      }

      if( FD_UNLIKELY( hdr->data_len > 10485760UL ) ) {
        cur += 512UL;
        continue;
      }

      ulong rec_sz = fd_ulong_align_up( sizeof(snap_acc_hdr_t) + hdr->data_len, 8UL );
      if( FD_UNLIKELY( cur + rec_sz > end ) ) break;

      FD_TEST( acc_cnt < max_accs );
      accs[ acc_cnt ].hdr    = hdr;
      accs[ acc_cnt ].rec_sz = rec_sz;
      accs[ acc_cnt ].hash   = fd_accdb_hash( hdr->pubkey.uc, 0UL );

      total_raw += rec_sz;
      acc_cnt++;
      cur += rec_sz;
    } else {
      break;
    }
  }

  FD_LOG_NOTICE(( "Parsed %lu accounts (%lu raw uncompressed bytes)", acc_cnt, total_raw ));

  uchar * buf_agave = (uchar *)malloc( total_raw );
  FD_TEST( buf_agave );

  uchar * p = buf_agave;
  for( ulong i=0UL; i<acc_cnt; i++ ) {
    fd_memcpy( p, accs[i].hdr, accs[i].rec_sz );
    p += accs[i].rec_sz;
  }

  qsort( accs, acc_cnt, sizeof(parsed_acc_t), acc_hash_cmp );

  uchar * buf_fd = (uchar *)malloc( total_raw );
  FD_TEST( buf_fd );

  p = buf_fd;
  for( ulong i=0UL; i<acc_cnt; i++ ) {
    fd_memcpy( p, accs[i].hdr, accs[i].rec_sz );
    p += accs[i].rec_sz;
  }

   /* hardcoded compress levels for level 1, which matches agave and firedancer */
  size_t comp_cap = ZSTD_compressBound( total_raw );
  uchar * comp_agave_buf = (uchar *)malloc( comp_cap );
  uchar * comp_fd_buf    = (uchar *)malloc( comp_cap );
  FD_TEST( comp_agave_buf && comp_fd_buf );

  size_t comp_agave_sz = ZSTD_compress( comp_agave_buf, comp_cap, buf_agave, total_raw, 1 );
  size_t comp_fd_sz    = ZSTD_compress( comp_fd_buf,    comp_cap, buf_fd,    total_raw, 1 );

  FD_TEST( !ZSTD_isError( comp_agave_sz ) );
  FD_TEST( !ZSTD_isError( comp_fd_sz ) );

  double ratio_agave = (double)total_raw / (double)comp_agave_sz;
  double ratio_fd    = (double)total_raw / (double)comp_fd_sz;
  long   delta_bytes = (long)comp_fd_sz - (long)comp_agave_sz;
  double delta_pct   = ( (double)delta_bytes / (double)comp_agave_sz ) * 100.0;

  FD_LOG_NOTICE(( "accounts: %lu, raw: %lu (%.2f MB), agave: %lu (%.3fx), fd: %lu (%.3fx), delta: %+ld (+%.2f%%)",
                  acc_cnt,
                  total_raw, (double)total_raw / (1024.0 * 1024.0),
                  (ulong)comp_agave_sz, ratio_agave,
                  (ulong)comp_fd_sz, ratio_fd,
                  delta_bytes, delta_pct ));

  free( comp_fd_buf );
  free( comp_agave_buf );
  free( buf_fd );
  free( buf_agave );
  free( accs );

  FD_LOG_NOTICE(( "pass" ));
  fd_halt();
  return 0;
}