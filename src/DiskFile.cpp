/* ES40 emulator.
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 *
 * WWW    : http://sourceforge.net/projects/es40
 * E-mail : camiel@camicom.com
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 * Although this is not required, the author would appreciate being notified of, 
 * and receiving any modifications you may make to the source code that might serve
 * the general public.
 */

/**
 * \file
 * Contains code to use a file as a disk image.
 *
 * $Id: DiskFile.cpp,v 1.15 2008/01/25 11:56:42 iamcamiel Exp $
 *
 * X-1.15       Camiel Vanderhoeven                             25-JAN-2008
 *      Create file if it doesn't exist and autocreate_size is specified.
 *
 * X-1.14       Camiel Vanderhoeven                             13-JAN-2008
 *      Use determine_layout in stead of calc_cylinders.
 *
 * X-1.13       Brian Wheeler                                   09-JAN-2008
 *      Put filename in disk model number (without path).
 *
 * X-1.12       Camiel Vanderhoeven                             09-JAN-2008
 *      Save disk state to state file.
 *
 * X-1.11       Camiel Vanderhoeven                             06-JAN-2008
 *      Set default blocksize to 2048 for cd-rom devices.
 *
 * X-1.10       Camiel Vanderhoeven                             06-JAN-2008
 *      Support changing the block size (required for SCSI, ATAPI).
 *
 * X-1.9        Camiel Vanderhoeven                             04-JAN-2008
 *      64-bit file I/O.
 *
 * X-1.8        Camiel Vanderhoeven                             02-JAN-2008
 *      Cleanup.
 *
 * X-1.7        Camiel Vanderhoeven                             28-DEC-2007
 *      Throw exceptions rather than just exiting when errors occur.
 *
 * X-1.6        Camiel Vanderhoeven                             28-DEC-2007
 *      Keep the compiler happy.
 *
 * X-1.5        Camiel Vanderhoeven                             20-DEC-2007
 *      Close files and free memory when the emulator shuts down.
 *
 * X-1.4        Camiel Vanderhoeven                             18-DEC-2007
 *      Byte-sized transfers for SCSI controller.
 *
 * X-1.3        Brian wheeler                                   17-DEC-2007
 *      Changed last cylinder number.
 *
 * X-1.2        Brian Wheeler                                   16-DEC-2007
 *      Fixed case of StdAfx.h.
 *
 * X-1.1        Camiel Vanderhoeven                             12-DEC-2007
 *      Initial version in CVS.
 **/

#include "StdAfx.h" 
#include "DiskFile.h"

CDiskFile::CDiskFile(CConfigurator * cfg, CSystem * sys, CDiskController * c, int idebus, int idedev) : CDisk(cfg,sys,c,idebus,idedev)
{

  filename = myCfg->get_text_value("file");
  if (!filename)
  {
    printf("%s: Disk has no file attached!\n",devid_string);
    throw((int)1);
  }
  
  if (read_only)
    handle = fopen(filename,"rb");
  else
    handle = fopen(filename,"rb+");
  if (!handle)
  {
    printf("%s: Could not open file %s!\n",devid_string,filename);
    int sz = myCfg->get_int_value("autocreate_size",0);
    if (!sz)
      FAILURE("File does not exist and no autocreate_size set");
    void * crt_buf;
    handle = fopen(filename,"wb");
    if (!handle)
      FAILURE("File does not exist and could not be created");
    crt_buf = calloc(1024,1024);
    for (int a = 0; a<sz; a++)
      fwrite(crt_buf,1024,1024,handle);
    fclose(handle);
    free(crt_buf);
    if (read_only)
      handle = fopen(filename,"rb");
    else
      handle = fopen(filename,"rb+");
    if (!handle)
      FAILURE("File created could not be opened");
    printf("%s: %d MB file %s created.\n",devid_string,sz,filename);
  }

  // determine size...
  fseek_large(handle,0,SEEK_END);
  byte_size=ftell_large(handle);
  fseek_large(handle,0,SEEK_SET);
  state.byte_pos = ftell_large(handle);

  sectors = 32;
  heads = 8;

  //calc_cylinders();
  determine_layout();

  model_number=myCfg->get_text_value("model_number",filename);

  // skip to the filename portion of the path.
  char *p = model_number;
#ifdef _WIN32
  char x = '\\';
#else
  char x = '/';
#endif
  while(*p) {
    if(*p==x) model_number=p+1;
    p++;
  }
  
  printf("%s: Mounted file %s, %" LL "d %d-byte blocks, %" LL "d/%d/%d.\n",devid_string,filename,byte_size/state.block_size,state.block_size,cylinders,heads,sectors);
}

CDiskFile::~CDiskFile(void)
{
  printf("%s: Closing file.\n",devid_string);
  fclose(handle);
}

bool CDiskFile::seek_byte(off_t_large byte)
{
  if (byte >=byte_size)
  {
    printf("%s: Seek beyond end of file!\n",devid_string);
    throw((int)1);
  }

  fseek_large(handle,byte,SEEK_SET);
  state.byte_pos = ftell_large(handle);

  return true;
}

size_t CDiskFile::read_bytes(void *dest, size_t bytes)
{
  size_t r;
  r = fread(dest,1,bytes,handle);
  state.byte_pos = ftell_large(handle);
  return r;
}

size_t CDiskFile::write_bytes(void * src, size_t bytes)
{
  if (read_only)
    return 0;

  size_t r;
  r = fwrite(src,1,bytes,handle);
  state.byte_pos = ftell_large(handle);
  return r;
}
