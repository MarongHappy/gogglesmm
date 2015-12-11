/*******************************************************************************
*                         Goggles Audio Player Library                         *
********************************************************************************
*           Copyright (C) 2013-2016 by Sander Jansen. All Rights Reserved      *
*                               ---                                            *
* This program is free software: you can redistribute it and/or modify         *
* it under the terms of the GNU General Public License as published by         *
* the Free Software Foundation, either version 3 of the License, or            *
* (at your option) any later version.                                          *
*                                                                              *
* This program is distributed in the hope that it will be useful,              *
* but WITHOUT ANY WARRANTY; without even the implied warranty of               *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                *
* GNU General Public License for more details.                                 *
*                                                                              *
* You should have received a copy of the GNU General Public License            *
* along with this program.  If not, see http://www.gnu.org/licenses.           *
********************************************************************************/
#include "ap_defs.h"
#include "ap_utils.h"
#include "ap_thread_queue.h"
#include "ap_wait_io.h"

#ifndef WIN32
#include <errno.h>
#endif

namespace ap {


WaitIO::WaitIO(FXIODevice * device,FXInputHandle w,FXTime tm) : FXIO(device->mode()),io(device),watch(w),timeout(tm) {
  }

WaitIO::~WaitIO() {
  close();
  }

FXbool WaitIO::isOpen() const {
  return io->isOpen();
  }

FXbool WaitIO::isSerial() const {
  return io->isSerial();
  }

FXlong WaitIO::position() const {
  return io->position();
  }

FXlong WaitIO::position(FXlong offset,FXuint from) {
  return io->position(offset,from);
  }

FXival WaitIO::writeBlock(const void* data,FXival count){
  FXival n;
  do {
    n = io->writeBlock(data,count);
    }
  while(n<0 && ((errno==EWOULDBLOCK || errno==EAGAIN) && wait(WaitWritable)==WaitHasIO));
  return n;
  }

FXival WaitIO::readBlock(void*data,FXival count) {
  FXival n;
  do {
    n = io->readBlock(data,count);
    }
  while(n<0 && ((errno==EWOULDBLOCK || errno==EAGAIN) && wait(WaitReadable)==WaitHasIO));
  return n;
  }


FXlong WaitIO::truncate(FXlong sz) {
  return io->truncate(sz);
  }

FXbool WaitIO::flush() {
  return io->flush();
  }

FXint WaitIO::eof() {
  return io->eof();
  }

FXlong WaitIO::size() {
  return io->size();
  }

FXbool WaitIO::close() {
  if (io) {
    io->close();
    delete io;
    io=nullptr;
    }
  return true;
  }


FXuint WaitIO::wait(FXuchar m) {
  return ap_wait(io->handle(),watch,timeout,m);
  }

ThreadIO::ThreadIO(FXIODevice * dev,ThreadQueue*queue,FXTime tm) : WaitIO(dev,queue->handle(),tm), fifo(queue) {
  }


FXuint ThreadIO::wait(FXuchar m) {
  FXuint w;
  do {
    w =  ap_wait(io->handle(),watch,timeout,m);
    }
  while(w==WaitHasInterrupt && !fifo->checkAbort());
  return w;
  }



}
