//********************************************************
/**
 * @file  AuthPAM.hh
 *
 * @brief 
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1	
 * @date 19/02/10 
 */
//********************************************************


#ifndef AUTHPAM_HH_
#define AUTHPAM_HH_

#if DLOPEN_PAM
#include <dlfcn.h>
#include "pamdl.h"
#else
#include <security/pam_appl.h>
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>



class AuthPAM
{
	/*
	 * Used to pass the username/password
	 * to the PAM conversation function.
	 */
	struct user_pass {
	  char username[128];
	  char password[128];
	  char common_name[128];
	};

	static int conv (int n, const struct pam_message **msg_array, struct pam_response **response_array, void *appdata_ptr);

  public:
	static int start ();
	static int authentificate (const char *username, const char *password, const char *service);
	static void stop();

};

#endif

