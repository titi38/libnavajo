//********************************************************
/**
 * @file  AuthPAM.cc
 *
 * @brief PAM authentification for Unix systems
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1        
 * @date 19/02/15
 */
//********************************************************

#include "libnavajo/LogRecorder.hh"
#include "libnavajo/AuthPAM.hh"


//------------------------------------------------------------------------------

int AuthPAM::start ()
{
#if DLOPEN_PAM
  static const char pam_so[] = "libpam.so";

  /*
   * Load PAM shared object
   */
  if (!dlopen_pam (pam_so))
  {
    LOG->append(ERROR, "PAM: Could not load PAM library " + std::string(pam_so) + std::string(dlerror()));

    #if DLOPEN_PAM
    dlclose_pam ();
    #endif
    return 1;
  }
#endif

  /*
   * Event loop
   */
  bool isStarted=true;

  return 0;
}

//------------------------------------------------------------------------------

void AuthPAM::stop ()
{
#if DLOPEN_PAM
  dlclose_pam ();
#endif
}

//------------------------------------------------------------------------------

/*
 * PAM conversation function
 */
int AuthPAM::conv (int n, const struct pam_message **msg_array, struct pam_response **response_array, void *appdata_ptr)
{
  const struct user_pass *up = ( const struct user_pass *) appdata_ptr;
  struct pam_response *aresp;
  int i;
  int ret = PAM_SUCCESS;

  *response_array = NULL;

  if (n <= 0 || n > PAM_MAX_NUM_MSG)
    return (PAM_CONV_ERR);
  if ((aresp = (pam_response*) calloc (n, sizeof *aresp)) == NULL)
    return (PAM_BUF_ERR);

  /* loop through each PAM-module query */
  for (i = 0; i < n; ++i)
  {
    const struct pam_message *msg = msg_array[i];
    aresp[i].resp_retcode = 0;
    aresp[i].resp = NULL;
  
    /* use PAM_PROMPT_ECHO_x hints */
    switch (msg->msg_style)
    {
    case PAM_PROMPT_ECHO_OFF:
      aresp[i].resp = strdup (up->password);
      if (aresp[i].resp == NULL)
        ret = PAM_CONV_ERR;
      break;

    case PAM_PROMPT_ECHO_ON:
      aresp[i].resp = strdup (up->username);
      if (aresp[i].resp == NULL)
        ret = PAM_CONV_ERR;
      break;

    case PAM_ERROR_MSG:
    case PAM_TEXT_INFO:
      break;

    default:
      ret = PAM_CONV_ERR;
      break;
    }
  }

  if (ret == PAM_SUCCESS)
    *response_array = aresp;
  return ret;
}

//------------------------------------------------------------------------------
/*
 * Return 1 if authenticated and 0 if failed.
 * Called once for every username/password
 * to be authenticated.
 */

int AuthPAM::authentificate(const char *username, const char *password, const char *service)
{
  pam_handle_t *pamh = NULL;
  int status = PAM_SUCCESS;
  int ret = 0;
  struct user_pass up;
  struct pam_conv conv;

  strncpy (up.username, username, 100);
  strncpy (up.password, password, 100);
  strncpy (up.common_name, "", 100);    

  /* Initialize PAM */
  conv.conv = AuthPAM::conv;
  conv.appdata_ptr = (void *)&up;
  status = pam_start (service, up.username, &conv, &pamh);
  if (status == PAM_SUCCESS)
  {
      /* Call PAM to verify username/password */
      status = pam_authenticate(pamh, 0);
      if (status == PAM_SUCCESS)
        status = pam_acct_mgmt (pamh, 0);
      if (status == PAM_SUCCESS)
        ret = 1;

    /* Close PAM */
    pam_end (pamh, status);      
  }

  return ret;
}

