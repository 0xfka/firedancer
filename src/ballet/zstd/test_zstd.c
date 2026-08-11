#include "fd_zstd.h"
#include "fd_zstd_private.h"
#include "../../util/fd_util.h"
#include <stdalign.h>
#include <stddef.h>
#include "../../util/sanitize/fd_fuzz.h"

#if !FD_HAS_ZSTD
#error "fd_compress requires Zstandard"
#endif

/* mem must be aligned */

FD_STATIC_ASSERT( alignof ( fd_zstd_dstream_t      )==FD_ZSTD_DSTREAM_ALIGN, layout );
FD_STATIC_ASSERT( offsetof( fd_zstd_dstream_t, mem )==FD_ZSTD_DSTREAM_ALIGN, layout );

/* Test vectors */

static uchar const test_zstd_comp_0[] =  /* zstd("AAAA") */
  { 0x28, 0xb5, 0x2f, 0xfd, 0x04, 0x58, 0x21, 0x00,
    0x00, 0x41, 0x41, 0x41, 0x41, 0x77, 0x3e, 0xc4,
    0x2b };

static uchar const test_zstd_comp_1[] =  /* zstd("ABCD") */
  { 0x28, 0xb5, 0x2f, 0xfd, 0x04, 0x58, 0x21, 0x00,
    0x00, 0x41, 0x42, 0x43, 0x44, 0x6e, 0x9c, 0x71,
    0x3f };

FD_IMPORT_BINARY(zstd_test_frame_scan, "multi_frame_hundreds.zst");

static void
test_frame_scanning( void ) {
  fd_zstd_frame_t arr[2048];
  if(FD_UNLIKELY(zstd_test_frame_scan_sz < 9)){
    FD_LOG_ALERT(("imported test file is not valid"));
  }
  ulong frame_cnt = fd_zstd_find_frame_boundaries((const ulong*)zstd_test_frame_scan, zstd_test_frame_scan_sz, arr, 2048);
  if(FD_UNLIKELY(frame_cnt == 0)){
    FD_LOG_ALERT(("fail"));
  }
  for(ulong i = 0; i < frame_cnt; i++){
    FD_LOG_NOTICE(("start: %lu size: %lu", arr[i].offset, arr[i].sz));
    // WIP, just checking things manually
    /*
     * NOTICE  08-12 13:36:09.697072 0         test_zstd.c(42): start: 0 size: 1048610
     NOTICE  08-12 13:36:09.697083 0         test_zstd.c(42): start: 1048610 size: 1048610
     * ➜  firedancer git:(frame_scan) ✗ hexdump -C -s 1048610 -n 16 multi_frame_hundreds.zst
     00100022  28 b5 2f fd 04 48 00 00  10 55 e9 d3 eb 43 a9 60  |(./..H...U...C.`|
     00100032
     ➜  firedancer git:(frame_scan) ✗ hexdump -C -s 1048606 -n 12 multi_frame_hundreds.zst
     0010001e  87 aa c8 4c 28 b5 2f fd  04 48 00 00              |...L(./..H..|
     0010002a
     ➜  firedancer git:(frame_scan) ✗ hexdump -C -s 2097216 -n 16 multi_frame_hundreds.zst
     00200040  9a fc fc bd                                       |....|
     00200044
     ➜  firedancer git:(frame_scan) ✗
     */
  }
  return;
  }
static void
test_decompress( void ) {
  FD_TEST( fd_zstd_dstream_align()==FD_ZSTD_DSTREAM_ALIGN );

  ulong window_sz = 1UL<<21;
  ulong mem_sz    = fd_zstd_dstream_footprint( window_sz );
  uchar mem[mem_sz];  /* Use VLA to assist AddressSanitizer */

  fd_zstd_dstream_t * dstream = fd_zstd_dstream_new( mem, window_sz );
  FD_TEST( dstream );
  FD_TEST( dstream->magic==FD_ZSTD_DSTREAM_MAGIC );
  FD_TEST( dstream->mem_sz + sizeof(fd_zstd_dstream_t) == mem_sz );

  /* Successful decompress */

  uchar   out[64];
  uchar * out_cur = out;
  for( ulong k=0UL; k<2UL; k++ ) {
    uchar const * in_cur = test_zstd_comp_0;
    int rc = fd_zstd_dstream_read( dstream,
                &in_cur,  test_zstd_comp_0+sizeof(test_zstd_comp_0),
                &out_cur, out+sizeof(out),
                NULL );
    FD_TEST( rc==-1 );
    FD_TEST( in_cur ==test_zstd_comp_0+sizeof(test_zstd_comp_0) );
  }
  FD_TEST( out_cur==out+8 );
  FD_TEST( 0==memcmp( out, "AAAAAAAA", 8 ) );

  /* Decompress input and output byte by byte */

  uchar const * in_cur = test_zstd_comp_1;
  out_cur = out;
  for( ulong k=0UL; k<2UL; k++ ) {
    for( ulong j=0UL; j<64UL; j++ ) {
      uchar * out_end = out_cur<out+8 ? out_cur+1 : out_cur;
      if( in_cur==test_zstd_comp_1+sizeof(test_zstd_comp_1) )
        in_cur = test_zstd_comp_1;  /* cycle over input message */
      int rc = fd_zstd_dstream_read( dstream, &in_cur, in_cur+1, &out_cur, out_end, NULL );
      FD_TEST( rc<=0 );
      if( rc==-1 ) break;
    }
  }
  FD_TEST( out_cur==out+8 );
  FD_TEST( 0==memcmp( out, "ABCDABCD", 8 ) );

  /* Decompress input byte by byte */

  in_cur = test_zstd_comp_1;
  out_cur = out;
  for( ulong k=0UL; k<2UL; k++ ) {
    for( ulong j=0UL; j<64UL; j++ ) {
      uchar * out_end = out_cur<out+8 ? out_cur+1 : out_cur;
      if( in_cur==test_zstd_comp_1+sizeof(test_zstd_comp_1) )
        in_cur = test_zstd_comp_1;  /* cycle over input message */
      int rc = fd_zstd_dstream_read( dstream, &in_cur, in_cur+1, &out_cur, out_end, NULL );
      FD_TEST( rc<=0 );
      if( rc==-1 ) break;
    }
  }
  FD_TEST( out_cur==out+8 );
  FD_TEST( 0==memcmp( out, "ABCDABCD", 8 ) );

  /* Decompress output byte by byte */

  in_cur = test_zstd_comp_1;
  out_cur = out;
  for( ulong j=0UL; j<64UL; j++ ) {
    FD_TEST( out_cur<out+4 );
    int rc = fd_zstd_dstream_read( dstream,
                 &in_cur, test_zstd_comp_1+sizeof(test_zstd_comp_1),
                 &out_cur, out_cur+1,
                 NULL );
    FD_TEST( rc<=0 );
    if( rc==-1 ) break;
  }
  FD_TEST( out_cur==out+4 );
  FD_TEST( 0==memcmp( out, "ABCD", 4 ) );

  /* Abort partial compress */

  do {
    in_cur = test_zstd_comp_1;
    out_cur = out;
    int rc = fd_zstd_dstream_read( dstream, &in_cur, in_cur+3, &out_cur, out_cur+3, NULL );
    FD_TEST( rc==0 );

    fd_zstd_dstream_reset( dstream );

    in_cur = test_zstd_comp_0;
    rc = fd_zstd_dstream_read( dstream,
             &in_cur,  test_zstd_comp_0+sizeof(test_zstd_comp_0),
             &out_cur, out+sizeof(out),
             NULL );
    FD_TEST( rc==-1 );
    FD_TEST( in_cur ==test_zstd_comp_0+sizeof(test_zstd_comp_0) );

    FD_TEST( out_cur==out+4 );
    FD_TEST( 0==memcmp( out, "AAAA", 4 ) );
  } while(0);

  FD_TEST( fd_zstd_dstream_delete( dstream )==mem );
  FD_TEST( dstream->magic==0UL );
}

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

int
main( int     argc,
      char ** argv ) {
  fd_boot( &argc, &argv );

  fd_zstd_peek_t _peek[1];
  FD_TEST( fd_zstd_peek( _peek, NULL, 0UL )==NULL );
  for( ulong j=0UL; j<=5UL; j++ )
    FD_TEST( fd_zstd_peek( _peek, test_zstd_comp_0, j )==NULL );
  for( ulong j=6UL; j<=sizeof(test_zstd_comp_0); j++ ) {
    memset( _peek, 0, sizeof(fd_zstd_peek_t) );
    FD_TEST( fd_zstd_peek( _peek, test_zstd_comp_0, j )==_peek );
    FD_TEST( ( _peek->window_sz          == (1UL<<21)  ) &
             ( _peek->frame_is_skippable == 0          ) &
             ( _peek->frame_content_sz   == ULONG_MAX  ) );
  }

  test_decompress();
  test_frame_scanning();

  for( int lvl=0; lvl<20; lvl++ ) {
    FD_LOG_INFO(( "ZSTD_estimateCCtxSize(%d) = %lu", lvl, ZSTD_estimateCCtxSize( lvl ) ));
  }

  FD_LOG_NOTICE(( "pass" ));
  fd_halt();
  return 0;
}
