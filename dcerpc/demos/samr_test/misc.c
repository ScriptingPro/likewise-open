#if HAVE_CONFIG_H
#include <config.h>
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <compat/dcerpc.h>
#include "misc.h"

void 
chk_dce_err(ecode, where, why, fatal)
     error_status_t ecode;
     char * where;
     char * why;
     unsigned int fatal;
{

  dce_error_string_t errstr;
  int error_status;                           
  
  if (ecode != error_status_ok)
  {
       dce_error_inq_text(ecode, (unsigned_char_p_t)errstr, &error_status); 
       if (error_status == error_status_ok)
	 printf("ERROR.  where = <%s> why = <%s> error code = 0x%x"
		"reason = <%s>\n",
		where, why, (unsigned32)ecode, errstr);
       else
	 printf("ERROR.  where = <%s> why = <%s> error code = 0x%x\n",
		where, why, (unsigned32)ecode);
       
       if (fatal) exit(1);
    }
}
