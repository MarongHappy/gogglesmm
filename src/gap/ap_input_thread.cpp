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
#include "ap_config.h"
#include "ap_defs.h"
#include "ap_utils.h"
#include "ap_pipe.h"
#include "ap_event.h"
#include "ap_format.h"
#include "ap_device.h"
#include "ap_buffer.h"
#include "ap_input_plugin.h"
#include "ap_event_private.h"
#include "ap_event_queue.h"
#include "ap_thread_queue.h"
#include "ap_packet.h"
#include "ap_engine.h"
#include "ap_thread.h"
#include "ap_thread_queue.h"
#include "ap_http.h"
#include "ap_input_thread.h"
#include "ap_reader_plugin.h"
#include "ap_decoder_thread.h"

#include "plugins/ap_file_plugin.h"
#include "plugins/ap_http_plugin.h"
#include "plugins/ap_cdda_plugin.h"
#include "plugins/ap_smb_plugin.h"

#ifndef WIN32
#include <errno.h>
#endif

namespace ap {

InputThread::InputThread(AudioEngine*e) : EngineThread(e),
  input(nullptr),
  reader(nullptr),
  streamid(0),
  use_mmap(false),
  state(StateIdle) {
  }


InputThread::~InputThread() {
  }

FXbool InputThread::init() {

  if (!EngineThread::init())
    return false;

  if (!packetpool.init(8192,20))
    return false;

  return true;
  }

void InputThread::free() {
  packetpool.free();
  EngineThread::free();
  }

/*
Event * InputThread::wait_for_io() {
  Event * event=nullptr;
  do {
    event=fifo.pop();
    if (event) return event;

    ap_wait(io.handle(),fifo.handle());
    }
  while(1);
  return nullptr;
  }
*/




FXbool InputThread::aborted() {
  return fifo.checkAbort();
  }

Event * InputThread::wait_for_event() {
  Event * event = fifo.pop();
  if (event==nullptr) {
    ap_wait(fifo.handle());
    event = fifo.pop();
    }
  FXASSERT(event);
  return event;
  }

Event * InputThread::wait_for_packet() {
  Event * event=nullptr;
  do {
    event=fifo.pop();
    if (event) return event;

    Packet * packet = packetpool.pop();
    if (packet) return packet;

    ap_wait(fifo.handle(),packetpool.handle());
    }
  while(1);
  return nullptr;
  }



/*
   event=fifo.pop();
  if (event) return event;





  do {
    event = fifo.pop();
    if (event)



    if (event) {
      ap_pipe_read_one(fifo.handle());
      return event;
      }

    event = DecoderPacket::get();
    if (event) {
      ap_pipe_read_one(DecoderPacket::handle());
      return event;
      }

    ap_wait(DecoderPacket::handle(),fifo.handle());
    }
  while(1);
  return nullptr;
  }
*/

Packet * InputThread::get_packet() {
  FXuchar type;
  do {
    type = fifo.peek();

    if (type!=Buffer && type!=AP_INVALID){
      return nullptr;
      }

    Packet * packet = packetpool.pop();
    if (packet) return packet;

    ap_wait(packetpool.handle(),fifo.handle());
    }
  while(1);
  return nullptr;
  }


#if 0
Event* InputThread::get_packet() {
  Event * event=nullptr;


  event = DecoderPacket::get();
  if (event) return event;



  if (fifo.peek()!=Invalid) return nullptr;

  FXint result = ap_wait(DecoderPacket::handle(),fifo.handle());
  //fxmessage("get packet result==%d\n",result);
  if (result==1) {
    event=DecoderPacket::get();
    }
  return event;
  }



#endif







FXint InputThread::run(){
  Event * event;

  ap_set_thread_name("ap_input");

  for (;;) {
    if (reader && state==StateProcessing)
      event = wait_for_packet();
    else
      event = wait_for_event();

    switch(event->type) {
      case Ctrl_Close     : ctrl_flush(true);
                            ctrl_close_input(true);
                            break;

      case Ctrl_Open_Flush: ctrl_flush();
      case Ctrl_Open      : ctrl_open_input(((ControlEvent*)event)->text);
                            break;

      case Ctrl_Quit      : ctrl_close_input(true);
                            engine->decoder->post(event,EventQueue::Flush);
                            return 0;
                            break;
      case Ctrl_Seek      : ctrl_seek(((CtrlSeekEvent*)event)->pos);
                            break;
      case End            : if (event->stream==streamid) {
                              ctrl_eos();
                              }
                            break;
      case Meta           : engine->decoder->post(event);
                            continue;
                            break;
      case AP_EOS         : GM_DEBUG_PRINT("[input] eos\n");
                            if (state!=StateError) {
                              engine->post(event);
                              continue;
                              }
                            break;
      case Buffer         :
        {
          Packet * packet = dynamic_cast<Packet*>(event);
          FXASSERT(reader);
          FXASSERT(packet);
          packet->stream = streamid;
          FXuint status = reader->process(packet);
          switch(status) {
            case ReadError    : GM_DEBUG_PRINT("[input] error\n");
                                ctrl_close_input();
                                set_state(StateError,true);
                                break;
            case ReadDone     : GM_DEBUG_PRINT("[input] done\n");
                                set_state(StateIdle);
                                break;
            case ReadRedirect : {GM_DEBUG_PRINT("[input] redirect\n");
                                FXStringList list;
                                reader->redirect(list);
                                ctrl_open_inputs(list);
                                }
                                break;
            default         : break;
            }
          continue; /* packet already released */
          break;
        }
      }
    Event::unref(event);
    }
  return 0;
  }



void InputThread::ctrl_eos() {
  GM_DEBUG_PRINT("[input] end of stream reached\n");
  if (state!=StateProcessing) {
    //ctrl_flush(true);
    ctrl_close_input(true);
    }
  }

void InputThread::ctrl_seek(FXdouble pos) {
  FXlong offset;
  if (reader && !input->serial() && reader->can_seek()) {
    offset = reader->seek_offset(pos);
    if (offset>=0 && reader->seek(offset)) {
      ctrl_seek_flush(offset);
      set_state(StateProcessing,true);
      }
    }
  }



InputPlugin* InputThread::open_input(const FXString & uri) {
  FXString scheme = FXURL::scheme(uri);

  if (input) {
    delete input;
    input=nullptr;
    }

  if (scheme=="file" || scheme.empty()) {
    FileInput * file = new FileInput(this);
    if (!file->open(uri)){
      delete file;
      return nullptr;
      }
    url=uri;
    return file;
    }
  else if (scheme=="http") {
    HttpInput * http = new HttpInput(this);
    if (!http->open(uri)){
      delete http;
      return nullptr;
      }
    url=uri;
    return http;
    }
#ifdef HAVE_CDDA
  else if (scheme=="cdda") {
    CDDAInput * cdda = new CDDAInput(this);
    if (!cdda->open(uri)) {
      delete cdda;
      return nullptr;
      }
    url=uri;
    return cdda;
    }
#endif
#ifdef HAVE_SMB
  else if (scheme=="smb") {
    SMBInput * smb = new SMBInput(this);
    if (!smb->open(uri)) {
      delete smb;
      return nullptr;
      }
    url=uri;
    return smb;
    }
#endif
  else {
    return nullptr;
    }
  }

ReaderPlugin* InputThread::open_reader() {
  /// FIXME try to reuse existing plugin
  if (reader) {
    delete reader;
    reader=nullptr;
    }
  return ReaderPlugin::open(engine,input->plugin());
  }


void InputThread::ctrl_open_inputs(const FXStringList & urls){
  for (FXint i=0;i<urls.no();i++){
    GM_DEBUG_PRINT("[input] ctrl_open_inputs[%d] %s\n",i,urls[i].text());
    if (urls[i].empty()) continue;

    /// Open Input
    input=open_input(urls[i]);
    if (input==nullptr) {
      if (aborted()) break;
      continue;
      }

    /// Open Reader
    reader = open_reader();
    if (reader==nullptr) continue;

    if (!reader->init(input))
      continue;

    streamid++;
    set_state(StateProcessing,true);
    return;
    }
  ctrl_close_input();
  set_state(StateIdle,true);
  }

void InputThread::ctrl_open_input(const FXString & uri) {
  GM_DEBUG_PRINT("[input] ctrl_open_input %s\n",uri.text());

  if (uri.empty()) {
    goto failed;
    }

  /// Open Input
  input=open_input(uri);
  if (input==nullptr) {
    engine->post(new ErrorMessage(FXString::value("Unable to open %s.",uri.text())));
    goto failed;
    }

  reader = open_reader();
  if (reader==nullptr) {
    engine->post(new ErrorMessage(FXString::value("No reader available for %s format.",ap_format_name(input->plugin()))));
    goto failed;
    }

  if (!reader->init(input)) {
    engine->post(new ErrorMessage(FXString::value("Failed to initialize reader")));
    goto failed;
    }

  streamid++;
  set_state(StateProcessing,true);
  return;
failed:
  ctrl_close_input();
  set_state(StateIdle,true);
  }



void InputThread::set_state(FXuchar s,FXbool notify) {
  if (state!=s) {
    state=s;
    switch(state) {
      case StateIdle      : GM_DEBUG_PRINT("[input] state = idle\n");       break;
      case StateProcessing: GM_DEBUG_PRINT("[input] state = processing\n"); break;
      case StateError     : GM_DEBUG_PRINT("[input] state = error\n");      break;
      }
    }

  /// Tell front end about the state.
  if (notify) {
    switch(state) {
      case StateError     :
      case StateIdle      : engine->post(new Event(AP_STATE_READY));   break;
      case StateProcessing: engine->post(new Event(AP_STATE_PLAYING)); break;
      default       : break;
      }
    }
  }


void InputThread::ctrl_close_input(FXbool notify) {
  GM_DEBUG_PRINT("[input] close input %d\n",notify);
  if (input) {
    delete input;
    input=nullptr;
    url.clear();
    }
  if (reader) {
    delete reader;
    reader=nullptr;
    }
  set_state(StateIdle,notify);
  }

void InputThread::ctrl_seek_flush(FXlong offset){
  GM_DEBUG_PRINT("[input] seek flush\n");
  engine->decoder->post(new FlushEvent(offset),EventQueue::Flush);
  }

void InputThread::ctrl_flush(FXbool close){
  GM_DEBUG_PRINT("[input] flush\n");
  engine->decoder->post(new FlushEvent(close),EventQueue::Flush);
  }

}
