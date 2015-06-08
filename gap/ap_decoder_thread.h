/*******************************************************************************
*                         Goggles Audio Player Library                         *
********************************************************************************
*           Copyright (C) 2010-2015 by Sander Jansen. All Rights Reserved      *
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
#ifndef DECODER_H
#define DECODER_H

namespace ap {

class AudioEngine;
class DecoderPlugin;
class Packet;
class ConfigureEvent;

class DecoderThread : public EngineThread {
protected:
  PacketPool      packetpool;
  DecoderPlugin * plugin;
protected:
  FXuint stream;
protected:
  void configure(ConfigureEvent*);
public:
  DecoderThread(AudioEngine*);

  FXuchar codec() const { return Codec::Invalid; }

  virtual FXint run();
  virtual FXbool init();
  virtual void free();

  Event * wait_for_packet();
  Packet * get_output_packet();
  Packet * get_decoder_packet();
  virtual ~DecoderThread();
  };

}
#endif


