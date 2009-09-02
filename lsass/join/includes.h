/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 * -*- mode: c, c-basic-offset: 4 -*- */

/*
 * Copyright Likewise Software
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.  You should have received a copy of the GNU General
 * Public License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * LIKEWISE SOFTWARE MAKES THIS SOFTWARE AVAILABLE UNDER OTHER LICENSING
 * TERMS AS WELL.  IF YOU HAVE ENTERED INTO A SEPARATE LICENSE AGREEMENT
 * WITH LIKEWISE SOFTWARE, THEN YOU MAY ELECT TO USE THE SOFTWARE UNDER THE
 * TERMS OF THAT SOFTWARE LICENSE AGREEMENT INSTEAD OF THE TERMS OF THE GNU
 * GENERAL PUBLIC LICENSE, NOTWITHSTANDING THE ABOVE NOTICE.  IF YOU
 * HAVE QUESTIONS, OR WISH TO REQUEST A COPY OF THE ALTERNATE LICENSING
 * TERMS OFFERED BY LIKEWISE SOFTWARE, PLEASE CONTACT LIKEWISE SOFTWARE AT
 * license@likewisesoftware.com
 */

#ifndef __INCLUDES_H__
#define __INCLUDES_H__

#include "config.h"
#include "lsasystem.h"
#include "lsadef.h"
#include "lsa/lsa.h"
#include "eventlog.h"

#include "lwmem.h"
#include <lwstr.h>
#include <lwkrb5.h>
#include "lsakrb5smb.h"
#include <uuid/uuid.h>
#include "lwsecurityidentifier.h"

#include "lsautils.h"
#include "lsaunistr.h"

#include "lsasrvutils.h"
#include "lwtime.h"
#include "lsaldap.h"

#include <lwps/lwps.h>
#include <lwnet.h>
#include <winerror-conv.h>

#include <lwrpc/LM.h>
#include <lwrpc/lsa.h>
#include <lwrpc/netlogon.h>
#include <lwrpc/samr.h>

#include <lwio/lwio.h>

#include <lsa/join.h>
#include "join_p.h"


#include <lwldap.h>
#include "lsaldap_p.h"

#include "externs.h"

#include <lber.h>
#include <lwrpc/krb5pac.h>

#endif /* __INCLUDES_H__ */
