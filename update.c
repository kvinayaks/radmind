/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#ifdef sun
#include <sys/mkdev.h>
#endif /* sun */
#ifdef __APPLE__
#include <sys/attr.h>
#endif /* __APPLE__ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>

#include "applefile.h"
#include "base64.h"
#include "update.h"
#include "code.h"
#include "radstat.h"
#include "transcript.h"
#include "progress.h"

extern int quiet;
extern int linenum;
extern int showprogress;

    int
update( const char *path, char *displaypath, int present, int newfile,
    struct stat *st, int tac, char **targv, struct applefileinfo *afinfo )
{
    int			timeupdated = 0;
    mode_t              mode;
    struct utimbuf      times;
    uid_t               uid;
    gid_t               gid;
    dev_t               dev;
    char		type;
    char		*d_target;
#ifdef __APPLE__
    char			fi_data[ FINFOLEN ];
    extern struct attrlist	setalist;
    static char                 null_buf[ 32 ] = { 0 };
#endif /* __APPLE__ */

    switch ( *targv[ 0 ] ) {
    case 'a':
    case 'f':
	if ( tac != 8 ) {
	    fprintf( stderr, "%d: incorrect number of arguments\n", linenum );
	    return( 1 );
	}

	mode = strtol( targv[ 2 ], (char **)NULL, 8 );

	times.modtime = atoi( targv[ 5 ] );
	if ( times.modtime != st->st_mtime ) {
	    times.actime = st->st_atime;
	    if ( utime( path, &times ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    timeupdated = 1;
	}
	break;

    case 'd':
	if (( tac != 5 )
#ifdef __APPLE__
		&& ( tac != 6 )
#endif /* __APPLE__ */
		) {
	    fprintf( stderr,
		"%d: incorrect number of arguments\n", linenum );
	    return( 1 );
	}

	mode = strtol( targv[ 2 ], (char **)NULL, 8 );

	if ( !present ) {
	    if ( mkdir( path, mode ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    newfile = 1;
	    if ( radstat( (char*)path, st, &type, afinfo ) < 0 ) {
		perror( path );
		return( 1 );
	    }
	    present = 1;
	}

#ifdef __APPLE__
	/* Check finder info */
	if ( tac == 6 ) {
	    memset( fi_data, 0, FINFOLEN );
	    base64_d( targv[ 5 ], strlen( targv[ 5 ] ), fi_data );
	} else {
	    memcpy( fi_data, null_buf, FINFOLEN );
	}
	if ( memcmp( afinfo->ai.ai_data, fi_data, FINFOLEN ) != 0 ) {
	    if ( setattrlist( path, &setalist,
		    fi_data, FINFOLEN, FSOPT_NOFOLLOW ) != 0 ) {
		fprintf( stderr,
		    "retrieve %s failed: Could not set attributes\n", path );
		return( -1 );
	    }
	}
#endif /* __APPLE__ */
	break;

    case 'h':
	if ( tac != 3 ) {
	    fprintf( stderr,
		"%d: incorrect number of arguments\n", linenum );
	    return( 1 );
	}
	if (( d_target = decode( targv[ 2 ] )) == NULL ) {
	    fprintf( stderr, "line %d: target path too long\n", linenum );
	    return( 1 );
	} 
	if ( link( d_target, path ) != 0 ) {
	    perror( path );
	    return( 1 );
	}
	if ( !quiet && !showprogress ) {
	    printf( "%s: hard linked to %s", displaypath, d_target);
	}
	goto done;

    case 'l':
	if ( tac != 3 ) {
	    fprintf( stderr,
		"%d: incorrect number of arguments\n", linenum );
	    return( 1 );
	}
	if ( present ) {
	    if ( unlink( path ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    present = 0;
	}
	if (( d_target = decode( targv[ 2 ] )) == NULL ) {
	    fprintf( stderr, "line %d: target path too long\n", linenum );
	    return( 1 );
	} 
	if ( symlink( d_target, path ) != 0 ) {
	    perror( path );
	    return( 1 );
	}
	if ( !quiet && !showprogress ) printf( "%s: symbolic linked to %s",
		displaypath, d_target );
	goto done;

    case 'p':
	if ( tac != 5 ) { 
	    fprintf( stderr,
		"%d: incorrect number of arguments\n", linenum );
	    return( 1 );
	}

	mode = strtol( targv[ 2 ], (char **)NULL, 8 ) | S_IFIFO;

	if ( !present ) {
	    if ( mkfifo( path, mode ) != 0 ){
		perror( path );
		return( 1 );
	    }
	    if ( lstat( path, st ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    present = 1;
	    newfile = 1;
	}
	break;

    case 'b':
    case 'c':
	if ( tac != 7 ) {
	    fprintf( stderr,
		"%d: incorrect number of arguments\n", linenum );
	    return( 1 );
	}

	mode = strtol( targv[ 2 ], (char **)NULL, 8 );

	if ( present && (( minor( st->st_rdev ) != atoi( targv[ 6 ] )) ||
		( major( st->st_rdev ) != atoi( targv[ 5 ] )))) {
	    if ( unlink( path ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    present = 0;
	}

	if ( !present ) {
#ifdef sun
	    if ( ( dev = makedev( (major_t)atoi( targv[ 5 ] ),
		   (minor_t)atoi( targv[ 6 ] ) ) ) == NODEV ) {
	       perror( path );
	       return( 1 );
	    }
#else /* !sun */
	    dev = makedev( atoi( targv[ 5 ] ), atoi( targv[ 6 ] ));
#endif /* sun */

	    if( *targv[ 0 ] == 'b' ) {
		mode |= S_IFBLK;
	    } else {
		mode |= S_IFCHR;
	    }

	    if ( mknod( path, mode, dev ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    if ( lstat( path, st ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    present = 1;
	    newfile = 1;
	}
	break;

    case 's':
    case 'D':
	if ( tac != 5 ) { 
	    fprintf( stderr,
		"%d: incorrect number of arguments\n", linenum );
	    return( 1 );
	}

	mode = strtol( targv[ 2 ], (char **)NULL, 8 );

	if ( !present ) {
	    fprintf( stderr, "%d: Warning: %c %s not created...continuing\n",
		    linenum, *targv[ 0 ], path );
	    return( 0 );
	}

	break;

    default :
	fprintf( stderr, "%d: Unknown type %s\n", linenum, targv[ 0 ] );
	return( 1 );
    }

    if ( !quiet && !showprogress ) {
	if ( newfile ) {
	    printf( "%s: created updating", displaypath );
	} else {
	    printf( "%s: updating", displaypath );
	}
    }

    /* check uid & gid */
    uid = atoi( targv[ 3 ] );
    gid = atoi( targv[ 4 ] );
    if ( uid != st->st_uid || gid != st->st_gid ) {
	if ( chown( path, uid, gid ) != 0 ) {
	    perror( path );
	    return( 1 );
	}
	if ( uid != st->st_uid ) {
	    if ( !quiet && !showprogress ) printf( " uid" );
	}
	if ( gid != st->st_gid ) {
	    if ( !quiet && !showprogress ) printf( " gid" );
	}
    }
    /* chmod after chown to preserve S_ISUID and S_ISGID mode bits */
    if ( mode != ( T_MODE & st->st_mode )) {
	if ( chmod( path, mode ) != 0 ) {
	    perror( path );
	    return( 1 );
	}
	if ( !quiet && !showprogress ) printf( " mode" );
    }

    if ( timeupdated && !showprogress && !quiet ) {
	printf( " time" );
    }

done:

    if ( !quiet && !showprogress ) printf( "\n" );
    if ( showprogress ) {
	progressupdate( UPDATEUNIT, displaypath );
    }

    return( 0 );
}
