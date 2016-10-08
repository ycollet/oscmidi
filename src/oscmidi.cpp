/*
  This file is part of oscmidi.

  Copyright (C) 2011  Jari Suominen
  Copyright (C) 2014  Yann Collette

  ttymidi is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  ttymidi is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with ttymidi.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <signal.h>
#include <pthread.h>
#include <unistd.h> // for sleep

#include <iostream>
#include <vector>
#include <string>
#include <cstring>

#include <lo/lo.h>
#include <alsa/asoundlib.h>

using namespace std;

#define OSCMIDI_VERSION "0.666"
#define OSCMIDI_CONTACT "satan@heaven.mil"

bool run;
int port_out_id;
snd_seq_t *seq;

// +-----------------+
// | Program options |
// +-----------------+

typedef struct _arguments {
  int  verbose;
  int  exit;
  int  tcp;
  string ip;
  string midihw;
  string name;
  string port;
  string rport;
} arguments_t;

static _arguments arguments;

void process_options(int in_argc, char **in_argv) {
  vector<string> args(in_argv + 1, in_argv + in_argc);
  
  // Initialization of the default values
  arguments.ip      = string("127.0.0.1");
  arguments.port    = string("7000");
  arguments.rport   = string("");
  arguments.name    = string("oscmidi");
  arguments.midihw  = string("default");
  arguments.verbose = 0;
  arguments.tcp     = 0;
  arguments.exit    = 0;
  
  // Loop over command-line args
  for (vector<string>::iterator i = args.begin(); i != args.end(); ++i) {
    if (*i == "-h" || *i == "--help") {
      cout << "oscmidi " << OSCMIDI_VERSION << " - " << OSCMIDI_CONTACT << endl;
      cout << "oscmidi - Connecting to ALSA MIDI clients with OSC MIDI!" << endl;
      cout << "oscmidi parameters:" << endl;
      cout << "   --ip, -i <IP>: IP address (or URL, for example foo.dyndns.org) where OSC messages are sent. Default = " << arguments.ip << endl;
      cout << "   --sport, -p <SEND_PORT>: Port where OSC messages are sent. Default = " << arguments.port << endl;
      cout << "   --rport, -r <RECEIVING_PORT>: Port for incoming OSC traffic. Default is port defined with -p" << endl;
      cout << "   --name, -n <NAME>:  Name of Alsa MIDI client created. Default = " << arguments.name << endl;
      cout << "   --hw, -w <NAME>:  Name of Alsa MIDI opened. Default = " << arguments.midihw << endl;
      cout << "   --verbose, -v: For debugging: Produce verbose output" << endl;
      cout << "   --tcp, -t: Will use TCP instead of UDP" << endl;
      
      arguments.exit = 1;
    } else if ((*i == "-i") || (*i == "--ip")) {
      arguments.ip = *++i;
    } else if ((*i == "-p") || (*i == "--sport")) {
      arguments.port = *++i;
    } else if ((*i == "-r") || (*i == "--rport")) {
      arguments.rport = *++i;
    } else if ((*i == "-n") || (*i == "--name")) {
      arguments.name = *++i;
    } else if ((*i == "-w") || (*i == "--hw")) {
      arguments.midihw = *++i;
    } else if ((*i == "-v") || (*i == "--verbose")) {
      arguments.verbose = 1;
    } else if ((*i == "-t") || (*i == "--tcp")) {
      arguments.tcp = 1;
    } else {
      cerr << "Wrong argument: " << *i << endl;
    }
  }
}

void printTime() {
  time_t currentTime;

  // get and print the current time
  time (&currentTime); // fill now with the current time

  struct tm * ptm= localtime(&currentTime);

  cout << (ptm->tm_year+1900) << "/";
  if ((ptm->tm_mon)+1 < 10) cout << 0;
  
  cout << ((ptm->tm_mon)+1) << "/";
  if (ptm->tm_mday < 10) cout << 0;
  
  cout << ptm->tm_mday << " ";
  if (ptm->tm_hour < 10) cout << 0;
  
  cout << ptm->tm_hour << ":";
  if (ptm->tm_min < 10) cout << 0;
  
  cout << ptm->tm_min << ":";
  if (ptm->tm_sec < 10) cout << 0;
  
  cout << ptm->tm_sec << " ";
}

// +------------+
// | MIDI stuff |
// +------------+

int open_seq(snd_seq_t** seq) 
{
  int port_out_id = -1, port_in_id = -1; // actually port_in_id is not needed nor used anywhere

  if (snd_seq_open(seq, arguments.midihw.c_str(), SND_SEQ_OPEN_DUPLEX, 0) < 0) {
    cerr << "Error opening ALSA sequencer." << endl;
    exit(1);
  }

  snd_seq_set_client_name(*seq, arguments.name.c_str());

  if ((port_out_id = snd_seq_create_simple_port(*seq, "MIDI out",
						SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
						SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
    cerr << "Error creating output sequencer port." << endl;
  }

  if ((port_in_id = snd_seq_create_simple_port(*seq, "MIDI in",
					       SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
					       SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
    cerr << "Error creating input sequencer port." << endl;
  }
  
  return port_out_id;
}

void send_midi_action_as_osc(snd_seq_t* seq_handle, lo_address * transmit_socket) 
{
  snd_seq_event_t* ev = NULL;

  do 
    {
      snd_seq_event_input(seq_handle, &ev);

      if (arguments.verbose) printTime();

      switch (ev->type) 
	{
	case SND_SEQ_EVENT_NOTEOFF:  
	  if (arguments.verbose) {
	    cout << "\tAlsa  Note off         \t" << (int)ev->data.control.channel;
	    cout << "\t" << (int)ev->data.note.note << "\t" << (int)ev->data.note.velocity;
	  }
	  
	  if (lo_send(*transmit_socket, "/oscmidi/noteoff", "iii", (int)ev->data.control.channel, 
		                                                   (int)ev->data.note.note, 
		                                                   (int)ev->data.note.velocity) == -1) {
	    cerr << "\tOSC error " << lo_address_errno(*transmit_socket) << ": " << lo_address_errstr(*transmit_socket) << endl;
	  }
	  break;
	case SND_SEQ_EVENT_NOTEON:  
	  if (arguments.verbose) {
	    cout << "\tAlsa  Note on          \t" << (int)ev->data.control.channel;
	    cout << "\t" << (int)ev->data.note.note << "\t" << (int)ev->data.note.velocity;
	  }
	  
	  if (lo_send(*transmit_socket, "/oscmidi/noteon", "iii", (int)ev->data.control.channel, 
		                                                  (int)ev->data.note.note, 
		                                                  (int)ev->data.note.velocity) == -1) {
	    cerr << "\tOSC error " << lo_address_errno(*transmit_socket) << ": " << lo_address_errstr(*transmit_socket) << endl;
	  }
	  break;
	case SND_SEQ_EVENT_KEYPRESS: 
	  if (arguments.verbose) {
	    cout << "\tAlsa  Pressure change  \t" << (int)ev->data.control.channel;
	    cout << "\t" << (int)ev->data.note.note << "\t" << (int)ev->data.note.velocity;
	  }
	  
	  if (lo_send(*transmit_socket, "/oscmidi/keypress", "iii", (int)ev->data.control.channel, 
		                                                    (int)ev->data.note.note, 
		                                                    (int)ev->data.note.velocity) == -1) {
	    cerr << "\tOSC error " << lo_address_errno(*transmit_socket) << ": " << lo_address_errstr(*transmit_socket) << endl;
	  }
	  break;
	case SND_SEQ_EVENT_CONTROLLER: 
	  if (arguments.verbose) {
	    cout << "\tAlsa  Controller Change\t" << (int)ev->data.control.channel;
	    cout << "\t" << (int)ev->data.control.param << "\t" << (int)ev->data.control.value;
	  }
	  
	  if (lo_send(*transmit_socket, "/oscmidi/cc", "iii", (int)ev->data.control.channel,
		                                              (int)ev->data.control.param, 
		                                              (int)ev->data.control.value) == -1) {
	    cerr << "\tOSC error " << lo_address_errno(*transmit_socket) << ": " << lo_address_errstr(*transmit_socket) << endl;
	  }
	  break;   
	case SND_SEQ_EVENT_PGMCHANGE:
	  if (arguments.verbose) {
	    cout << "\tAlsa  Program Change   \t" << (int)ev->data.control.channel;
	    cout << "\t" << (int)ev->data.control.value;
	  }
	  
	  if (lo_send(*transmit_socket, "/oscmidi/pgmchange", "ii", (int)ev->data.control.channel, 
		                                                    (int)ev->data.control.value) == -1) {
	    cerr << "\tOSC error " << lo_address_errno(*transmit_socket) << ": " << lo_address_errstr(*transmit_socket) << endl;
	  }
	  break;  
	case SND_SEQ_EVENT_CHANPRESS: 
	  if (arguments.verbose) {
	    cout << "\tAlsa  Channel Pressure \t" << (int)ev->data.control.channel;
	    cout << "\t" << (int)ev->data.control.value;
	  }
	  
	  if (lo_send(*transmit_socket, "/oscmidi/chanpress", "ii", (int)ev->data.control.channel, 
		                                                    (int)ev->data.control.value) == -1) {
	    cerr << "\tOSC error " << lo_address_errno(*transmit_socket) << ": " << lo_address_errstr(*transmit_socket) << endl;
	  }
	  break;  
	case SND_SEQ_EVENT_PITCHBEND:
	  if (arguments.verbose) {
	    cout << "\tAlsa  Pitch bend       \t" << (int)ev->data.control.channel;
	    cout << "\t" << (int)ev->data.control.value;
	  }
	  
	  if (lo_send(*transmit_socket, "/oscmidi/pitchbend", "ii", (int)ev->data.control.channel, 
		                                                    (int)ev->data.control.value) == -1) {
	    cerr << "\tOSC error " << lo_address_errno(*transmit_socket) << ": " << lo_address_errstr(*transmit_socket) << endl;
	  }  
	  break;
	case SND_SEQ_EVENT_START:
	  if (arguments.verbose) {
	    cout << "\tAlsa  Start";
	  }
	  if (lo_send(*transmit_socket, "/oscmidi/start", "i", 1) == -1) {
	    cerr << "\tOSC error " << lo_address_errno(*transmit_socket) << ": " << lo_address_errstr(*transmit_socket) << endl;
	  }  
	  break;
	case SND_SEQ_EVENT_CONTINUE:
	  if (arguments.verbose) {
	    cout << "\tAlsa  Continue";
	  }
	  if (lo_send(*transmit_socket, "/oscmidi/continue", "i", 1) == -1) {
	    cerr << "\tOSC error " << lo_address_errno(*transmit_socket) << ": " << lo_address_errstr(*transmit_socket) << endl;
	  }  
	  break;
	case SND_SEQ_EVENT_STOP:
	  if (arguments.verbose) {
	    cout << "\tAlsa  Stop";
	  }
	  if (lo_send(*transmit_socket, "/oscmidi/stop", "i", 1) == -1) {
	    cerr << "\tOSC error " << lo_address_errno(*transmit_socket) << ": " << lo_address_errstr(*transmit_socket) << endl;
	  }  
	  break;
	default:
	  if (arguments.verbose)
	    cout << "\tAlsa  Unknown command  \t";
	  break;
	}
      
      if (arguments.verbose) cout << endl;
      
      snd_seq_free_event(ev);
      
    } while (snd_seq_event_input_pending(seq_handle, 0) > 0);
}

void* read_midi_from_alsa(void * seq) 
{
  snd_seq_t* seq_handle = (snd_seq_t*)seq;
  int npfd = snd_seq_poll_descriptors_count(seq_handle, POLLIN);
  struct pollfd* pfd = (struct pollfd*) alloca(npfd * sizeof(struct pollfd));
  lo_address transmit_socket;
  
  snd_seq_poll_descriptors(seq_handle, pfd, npfd, POLLIN);	
  
  if (arguments.tcp) {
    transmit_socket = lo_address_new_with_proto(LO_TCP, arguments.ip.c_str(), arguments.port.c_str());
  } else {
    transmit_socket = lo_address_new(arguments.ip.c_str(), arguments.port.c_str());
  }
  
  if (arguments.verbose) {
    cout << "Sending MIDI to " << arguments.ip << ":" << arguments.port << " using";
    if (arguments.tcp)
      cout << " TCP." << endl;
    else
      cout << " UDP." << endl;
  }    
  
  while (run) 
    {
      if (poll(pfd,npfd, 100) > 0) 
	{
	  send_midi_action_as_osc(seq_handle, &transmit_socket);
	}
    }
  
  if (arguments.verbose) cout << "Stopping [Alsa]->[OSC] communication..." << endl;
  
  return NULL;
}

void exit_cli(int sig)
{
  run = false;
}

void error(int num, const char *msg, const char *path)
{
  cout << "liblo server error " << num << " in path " << path << ": " << msg << endl << flush;
}

int process_incoming_osc(const char *path, const char *types, lo_arg ** argv,
			 int argc, void *data, void *user_data) {
  try {
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_direct(&ev);
    snd_seq_ev_set_source(&ev, port_out_id);
    snd_seq_ev_set_subs(&ev);  
    
    if (arguments.verbose) printTime();
    
    if (std::strcmp(path, "/oscmidi/noteoff") == 0 && argc == 3) {
      snd_seq_ev_set_noteoff(&ev, argv[0]->i, argv[1]->i, argv[2]->i);
      if (arguments.verbose) {
	cout << "\tOSC  Note off         \t" << (int)ev.data.control.channel;
	cout << "\t" << (int)ev.data.note.note << "\t" << (int)ev.data.note.velocity << endl;
      }
    } else if (std::strcmp(path, "/oscmidi/noteon") == 0 && argc == 3) {
      snd_seq_ev_set_noteon(&ev, argv[0]->i, argv[1]->i, argv[2]->i);
      if (arguments.verbose) {
	cout << "\tOSC  Note on         \t" << (int)ev.data.control.channel;
	cout << "\t" << (int)ev.data.note.note << "\t" << (int)ev.data.note.velocity << endl;
      }
    } else if (std::strcmp(path, "/oscmidi/keypress") == 0 && argc == 3) {
      snd_seq_ev_set_keypress(&ev, argv[0]->i, argv[1]->i, argv[2]->i);
      if (arguments.verbose) {
	cout << "\tOSC  Pressure change       \t" << (int)ev.data.control.channel;
	cout << "\t" << (int)ev.data.note.note << "\t" << (int)ev.data.note.velocity << endl;
      }             				
    } else if (std::strcmp(path, "/oscmidi/cc") == 0 && argc == 3) {
      if(types[2] == 'i')
        snd_seq_ev_set_controller(&ev, argv[0]->i, argv[1]->i, argv[2]->i);
      else if(types[2] == 'f')
        snd_seq_ev_set_controller(&ev, argv[0]->i, argv[1]->i, argv[2]->f);
      if (arguments.verbose) {
	cout << "\tOSC  Controller Change\t" << (int)ev.data.control.channel;
	cout << "\t" << (int)ev.data.control.param << "\t" << (int)ev.data.control.value << endl;
      }             				
    } else if (std::strcmp(path, "/oscmidi/pgmchange") == 0 && argc == 2) {
      snd_seq_ev_set_pgmchange(&ev, argv[0]->i, argv[1]->i);
      if (arguments.verbose) {
	cout << "\tOSC  Program Change   \t" << (int)ev.data.control.channel;
	cout << "\t" << (int)ev.data.control.value << endl;    				
      }
    } else if (std::strcmp(path, "/oscmidi/chanpress") == 0 && argc == 2) {
      snd_seq_ev_set_chanpress(&ev, argv[0]->i, argv[1]->i);
      if (arguments.verbose) {
	cout << "\tOSC  Channel Pressure \t" << (int)ev.data.control.channel;
	cout << "\t" << (int)ev.data.control.value << endl;
      }
    } else if (std::strcmp(path, "/oscmidi/pitchbend") == 0 && argc == 2) {
      snd_seq_ev_set_pitchbend(&ev, argv[0]->i, (argv[1]->i - 8192));
      if (arguments.verbose) {
	cout << "\tOSC  Pitch bend       \t" << (int)ev.data.control.channel;
	cout << "\t" << (int)ev.data.control.value << endl;
      }
    } else if (std::strcmp(path, "/oscmidi") == 0 ) {
      if ( std::strcmp(&(argv[0]->s), "pitchbend" ) == 0) {
	snd_seq_ev_set_pitchbend(&ev, argv[1]->i, argv[2]->i);
	if (arguments.verbose) {
	  cout << "\tOSC  Pitch bend (depr) \t" << (int)ev.data.control.channel;
	  cout << "\t" << (int)ev.data.control.value << endl;
	}
      } else {
	if (arguments.verbose) {
	  cout << "\tOSC  Deprecated API    \t" << (int)ev.data.control.channel;
	  cout << "\t" << (int)ev.data.control.value << endl; 
	}
      }
    } else {
      cout << "\tOSC received '" << path << "' message, which is not oscmidi-message!" << endl;
      return 1;
    }
    
    snd_seq_event_output_direct(seq, &ev);
    snd_seq_drain_output(seq);
  } catch ( ... ) {
    cout << "error while parsing message: " << endl;
  }
  
  return 1;
}

// +--------------+ 
// | Main program |
// +--------------+

int main(int argc, char** argv)
{
  /* 
   * read command line arguments
   */
  
  process_options(argc, argv);
  
  if (arguments.exit) return 0;
  
  /*
   * Open MIDI ports
   */
  
  port_out_id = open_seq(&seq);
  
  if (arguments.verbose) {
    cout << endl;
    cout << "oscmidi  Copyright (C) 2011  Jari Suominen" << endl;
    cout << "This program comes with ABSOLUTELY NO WARRANTY." << endl;
    cout << "This program is free software: you can redistribute it and/or modify" << endl;
    cout << "it under the terms of the GNU General Public License as published by" << endl;
    cout << "the Free Software Foundation, either version 3 of the License, or" << endl;
    cout << "(at your option) any later version." << endl << endl;
  }
  
  /* Starting thread that is polling alsa midi in port */
  pthread_t midi_out_thread;
  run = true;
  pthread_create(&midi_out_thread, NULL, read_midi_from_alsa, (void*)seq);	
  
  /*
   * OSC In
   */
  
  if (arguments.rport.size() == 0) {
    arguments.rport = arguments.port;
  }
  
  lo_server_thread st;
  if (arguments.tcp) {
    st = lo_server_thread_new_with_proto(arguments.rport.c_str(), LO_TCP, error);
  } else {
    st = lo_server_thread_new_with_proto(arguments.rport.c_str(), LO_UDP, error);
  }
  
  /* add method that will match any path and args */
  lo_server_thread_add_method(st, NULL, NULL, process_incoming_osc, NULL);
  lo_server_thread_start(st);
  
  if (arguments.verbose) {
    cout << "Waiting for OSC messages at port " << arguments.rport << " using";
    if (arguments.tcp)
      cout << " TCP." << endl;
    else
      cout << " UDP." << endl;
  }
  
  signal(SIGINT,  exit_cli);
  signal(SIGTERM, exit_cli);
  
  /* Here we wait until ctrl-c or kill signal will be sent... */
  
  run = true;
  
  while (run) { sleep(1000); }
  
  /* Destruction routine */
  if (arguments.verbose)
    cout << "\r   " << endl << "Stopping [OSC]->[Alsa] communication..." << endl;
  
  run = false; 		// Alsa thread will poll this...
  void* status = NULL;
  pthread_join(midi_out_thread, &status);
  lo_server_thread_stop(st);
  lo_server_thread_free(st);
  usleep(1000); // Attempt to make sure that there is time to free the port. But might not help at all in this!
  
  if (arguments.verbose) 
    cout << "Bie!" << endl;
  
  cout << endl;
  
  return 0;
}
