/*
 *  MAST: Multicast Audio Streaming Toolkit
 *
 *  By Nicholas J. Humfrey <njh@ecs.soton.ac.uk>
 *
 *  Copyright (C) 2003-2007 University of Southampton
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  $Id:$
 */

#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include <ortp/ortp.h>


#include "config.h"
#include "mast.h"


/* Globals */
static int mast_running = TRUE;
static char* mast_tool_name = NULL;



/* oRTP callbacks */
static void ssrc_changed_cb(RtpSession *session)
{
	MAST_INFO("The ssrc has changed");
}

static void pt_changed_cb(RtpSession *session)
{
	MAST_INFO("The ssrc has changed");
}

static void network_error_cb(RtpSession *session, const char* msg)
{
	MAST_INFO("Network error: %s", msg);
}


/* Initialise the oRTP library */
RtpSession *mast_init_ortp( char* tool_name, int mode )
{
	RtpSession * session = NULL;
	int log_level = ORTP_WARNING|ORTP_ERROR|ORTP_FATAL;
	
	// Store the tool name
	mast_tool_name = tool_name;
	
	// Initialise the oRTP library
	ortp_init();
	ortp_scheduler_init();

	
	// Set the logging message level
#ifdef DEBUGGING
	MAST_DEBUG( "Compiled with debugging enabled" );
	log_level |= ORTP_DEBUG;
	log_level |= ORTP_MESSAGE;
#endif	
	ortp_set_log_level_mask(log_level);


	// Create RTP session
	session = rtp_session_new( mode );
	if (session==NULL) {
		MAST_FATAL( "Failed to create oRTP session.\n" );	
	}

	// Configure the RTP session
	rtp_session_set_scheduling_mode(session, TRUE);
	rtp_session_set_blocking_mode(session, TRUE);
	rtp_session_set_multicast_loopback(session, TRUE);

	// Callbacks
	rtp_session_signal_connect(session,"ssrc_changed",(RtpCallback)ssrc_changed_cb, 0);
	rtp_session_signal_connect(session,"payload_type_changed",(RtpCallback)pt_changed_cb, 0);
	rtp_session_signal_connect(session,"payload_type_changed",(RtpCallback)network_error_cb, 0);
//	rtp_session_signal_connect(session,"ssrc_changed",(RtpCallback)rtp_session_reset,0);


	// Set RTCP parameters
	mast_set_source_sdes( session );

	return session;
}


/* De-initialise the oRTP library */
void mast_deinit_ortp( RtpSession *session )
{

#ifdef DEBUGGING
	ortp_global_stats_display();
#endif

	rtp_session_uninit( session );
	ortp_exit();	

}


/* Get string of the FQDN for this host */
char* mast_gethostname()
{
	char hostname[ HOST_NAME_MAX ];
	char domainname[ DOMAIN_NAME_MAX ];
	struct hostent	*hp;
	
	if (gethostname( hostname, HOST_NAME_MAX ) < 0) {
		// Failed
		return NULL;
	}
	
	// If it contains a dot, then assume it is a FQDN
    if (strchr(hostname, '.') != NULL)
		return strdup( hostname );

    // Try resolving the hostname into a FQDN
    if ( (hp = gethostbyname( hostname )) != NULL ) {
    	if (strchr(hp->h_name, '.') != NULL)
    		return strdup( hp->h_name );
    }

	// Try appending our domain name
	if ( getdomainname( domainname, DOMAIN_NAME_MAX) == 0
	     && strlen( domainname ) )
	{
		int fqdn_len = strlen( hostname ) + strlen( domainname ) + 2;
		char *fqdn = malloc( fqdn_len );
		snprintf( fqdn, fqdn_len, "%s.%s", hostname, domainname );
		return fqdn;
	}


	// What else can we try?
	return NULL;
}


/* Get the name of the current tool */
char* mast_get_tool_name()
{
	if (mast_tool_name) return mast_tool_name;
	else return "Unknown Tool";
}



/* Set the RTCP session description  */
void mast_set_source_sdes( RtpSession *session )
{
	char cname[ STR_BUF_SIZE ];
	char tool[ STR_BUF_SIZE ];
	char *hostname=NULL;

	hostname = mast_gethostname();
	snprintf( cname, STR_BUF_SIZE, "%s@%s", PACKAGE_NAME, hostname );
	snprintf( tool, STR_BUF_SIZE, "%s (%s/%s)", mast_get_tool_name(), PACKAGE_NAME, PACKAGE_VERSION );
	free( hostname );
	
	rtp_session_set_source_description(
		session,			// RtpSession*
		cname,				// CNAME
		NULL,				// name
		NULL,				// email
		NULL,				// phone
		NULL,				// loc
		tool,				// tool
		NULL				// note
	);

}


/* Handle an error and store the error message */
void mast_message_handler( int level, const char* file, int line, char *fmt, ... )
{
	va_list args;
	FILE *fd = stderr;
	
	// If debugging is enabled then display the filename and line number
#ifdef DEBUGGING
	fprintf( fd, "[%s:%d] ", file, line );
#endif
	
	// Display the error message
	switch( level ) {
		case MSG_LEVEL_DEBUG:	fprintf( fd, "Debug: " ); break;
		case MSG_LEVEL_INFO:	fprintf( fd, "Info: " ); break;
		case MSG_LEVEL_WARNING:	fprintf( fd, "Warning: " ); break;
		case MSG_LEVEL_ERROR:	fprintf( fd, "Error: " ); break;
		case MSG_LEVEL_FATAL:	fprintf( fd, "Fatal Error: " ); break;
	}
	
	// Display the message
	va_start( args, fmt );
	vfprintf( fd, fmt, args );
	fprintf( fd, ".\n" );
	va_end( args );
	
	// If it is an error, then stop
	if (level==MSG_LEVEL_FATAL) {
		exit(-1);
	}
}



/* Signal callback handler */
static void mast_termination_handler(int signum)
{
	mast_running = FALSE;
	switch(signum) {
		case SIGTERM:	fprintf(stderr, "Got termination signal.\n"); break;
		case SIGINT:	fprintf(stderr, "Got interupt signal.\n"); break;
	}
}

/* Returns true if the programme should keep running */
int mast_still_running()
{
	return mast_running;
}

/* Setup signal handlers */
void mast_setup_signals()
{
	// Setup special handling of signals
	signal(SIGTERM, mast_termination_handler);
	signal(SIGINT, mast_termination_handler);
	//signal(SIGHUP, mast_termination_handler);

	// Set this 
	mast_running = TRUE;

}



/* Parse a DSCP class name into an integer value */
int mast_parse_dscp( const char* value )
{

	if (!strcmp(value,"BE"))         return 0x00;
	
	else if (!strcmp(value,"CS0"))   return 0x00;
	else if (!strcmp(value,"CS1"))   return 0x08;
	else if (!strcmp(value,"CS2"))   return 0x10;
	else if (!strcmp(value,"CS3"))   return 0x18;
	else if (!strcmp(value,"CS4"))   return 0x20;
	else if (!strcmp(value,"CS5"))   return 0x28;
	else if (!strcmp(value,"CS6"))   return 0x30;
	else if (!strcmp(value,"CS7"))   return 0x38;
	
	else if (!strcmp(value,"AF11"))  return 0x0A;
	else if (!strcmp(value,"AF12"))  return 0x0C;
	else if (!strcmp(value,"AF13"))  return 0x0E;
	else if (!strcmp(value,"AF21"))  return 0x12;
	else if (!strcmp(value,"AF22"))  return 0x14;
	else if (!strcmp(value,"AF23"))  return 0x16;
	else if (!strcmp(value,"AF31"))  return 0x1a;
	else if (!strcmp(value,"AF32"))  return 0x1C;
	else if (!strcmp(value,"AF33"))  return 0x1E;
	else if (!strcmp(value,"AF41"))  return 0x22;
	else if (!strcmp(value,"AF42"))  return 0x24;
	else if (!strcmp(value,"AF43"))  return 0x26;
	
	else if (!strcmp(value,"EF"))    return 0x2E;

	// Accept integers too
	return atoi( value );
}


