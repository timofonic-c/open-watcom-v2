/****************************************************************************
*
*                            Open Watcom Project
*
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  Librarian interface to ORL.
*
****************************************************************************/


#include "wlib.h"

#include "clibext.h"


#define ORL_FID2OF( fid )   ((obj_file *)(fid))
#define ORL_OF2FID( of )    ((orl_file_id)(of))

static orl_handle       ORLHnd;

static void *ObjRead( orl_file_id fid, size_t len )
/*************************************************/
{
    buf_list    *buf;

    buf = MemAlloc( len + sizeof( buf_list ) - 1 );
    if( LibRead( ORL_FID2OF( fid )->hdl, buf->buf, len ) != len ) {
        MemFree( buf );
        return( NULL );
    }
    buf->next = ORL_FID2OF( fid )->buflist;
    ORL_FID2OF( fid )->buflist = buf;
    return( buf->buf );
}

static long ObjSeek( orl_file_id fid, long pos, int where )
/*********************************************************/
{
    switch( where ) {
    case SEEK_SET:
        pos += ORL_FID2OF( fid )->offset;
        break;
    case SEEK_CUR:
        break;
    }
    LibSeek( ORL_FID2OF( fid )->hdl, pos, where );
    return( pos - ORL_FID2OF( fid )->offset );
}

static void *ObjAlloc( size_t size )
/**********************************/
{
    if( size == 0 )
        size = 1;
    return( MemAllocGlobal( size ) );
}

static void ObjFree( void *ptr )
/******************************/
{
    MemFreeGlobal( ptr );
}

void FiniObj( void )
/******************/
{
    ORLFini( ORLHnd );
}

void InitObj( void )
/******************/
{
    ORLSetFuncs( orl_cli_funcs, ObjRead, ObjSeek, ObjAlloc, ObjFree );

    ORLHnd = ORLInit( &orl_cli_funcs );
    if( ORLHnd == NULL ) {
        longjmp( Env , 1 );
    }
}

static obj_file *DoOpenObjFile( const char *name, libfile hdl, long offset )
/**************************************************************************/
{
    obj_file            *ofile;
    orl_file_format     format;

    ofile = MemAlloc( sizeof( *ofile ) );
    ofile->hdl = hdl;
    ofile->buflist = NULL;
    ofile->offset = offset;
    format = ORLFileIdentify( ORLHnd, ORL_OF2FID( ofile ) );
    switch( format ) {
    case ORL_COFF:
    case ORL_ELF:
        ofile->orl = ORLFileInit( ORLHnd, ORL_OF2FID( ofile ), format );
        if( Options.libtype == WL_LTYPE_MLIB ) {
            if( (ORLFileGetFlags( ofile->orl ) & VALID_ORL_FLAGS) != VALID_ORL_FLAGS ) {
                FatalError( ERR_NOT_LIB, "64-bit or big-endian", LibFormat() );
            }
        }
        if( ofile->orl == NULL ) {
            FatalError( ERR_CANT_OPEN, name, strerror( errno ) );
        }
        break;

    default: // case ORL_UNRECOGNIZED_FORMAT:
        ofile->orl = NULL;
        break;
    }
    return( ofile );
}

obj_file *OpenObjFile( const char *name )
/***************************************/
{
    libfile     hdl;

    hdl = LibOpen( name, LIBOPEN_READ );
    return( DoOpenObjFile( name, hdl, 0 ) );
}

obj_file *OpenLibFile( const char *name, libfile hdl )
/****************************************************/
{
    return( DoOpenObjFile( name, hdl, LibTell( hdl ) ) );
}

static void DoCloseObjFile( obj_file *ofile )
/*******************************************/
{
    buf_list    *list;
    buf_list    *next;

    if( ofile->orl != NULL ) {
        ORLFileFini( ofile->orl );
    }
    for( list = ofile->buflist; list != NULL; list = next ) {
        next = list->next;
        MemFree( list );
    }
    MemFree( ofile );
}

void CloseObjFile( obj_file *ofile )
/**********************************/
{
    libfile     hdl;

    hdl = ofile->hdl;
    DoCloseObjFile( ofile );
    LibClose( hdl );
}

void CloseLibFile( obj_file *ofile )
/**********************************/
{
    DoCloseObjFile( ofile );
}
