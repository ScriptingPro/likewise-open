/*
 * Copyright (c) Likewise Software.  All rights Reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the license, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.  You should have received a copy
 * of the GNU Lesser General Public License along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 *
 * LIKEWISE SOFTWARE MAKES THIS SOFTWARE AVAILABLE UNDER OTHER LICENSING
 * TERMS AS WELL.  IF YOU HAVE ENTERED INTO A SEPARATE LICENSE AGREEMENT
 * WITH LIKEWISE SOFTWARE, THEN YOU MAY ELECT TO USE THE SOFTWARE UNDER THE
 * TERMS OF THAT SOFTWARE LICENSE AGREEMENT INSTEAD OF THE TERMS OF THE GNU
 * LESSER GENERAL PUBLIC LICENSE, NOTWITHSTANDING THE ABOVE NOTICE.  IF YOU
 * HAVE QUESTIONS, OR WISH TO REQUEST A COPY OF THE ALTERNATE LICENSING
 * TERMS OFFERED BY LIKEWISE SOFTWARE, PLEASE CONTACT LIKEWISE SOFTWARE AT
 * license@likewisesoftware.com
 */

/*
 * Module Name:
 *
 *        lwma-main.c
 *
 * Abstract:
 *
 *        Likewise Message Archive tool
 *
 * Authors: Brian Koropoff (bkoropoff@likewisesoftware.com)
 *
 */

#include <lwmsg/lwmsg.h>
#include "util-private.h"
#include "data-private.h"

static
LWMsgStatus
archive_dump(
    const char* filename
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    LWMsgProtocol* protocol = NULL;
    LWMsgArchive* archive = NULL;
    LWMsgMessage message = LWMSG_MESSAGE_INITIALIZER;
    LWMsgDataContext* dcontext = NULL;
    char* text = NULL;
    unsigned int i = 0;

    BAIL_ON_ERROR(status = lwmsg_data_context_new(NULL, &dcontext));

    BAIL_ON_ERROR(status = lwmsg_protocol_new(NULL, &protocol));

    BAIL_ON_ERROR(status = lwmsg_archive_new(NULL, protocol, &archive));

    BAIL_ON_ERROR(status = lwmsg_archive_set_file(archive, filename, LWMSG_ARCHIVE_READ | LWMSG_ARCHIVE_SCHEMA, 0));

    BAIL_ON_ERROR(status = lwmsg_archive_open(archive));

    BAIL_ON_ERROR(status = lwmsg_data_print_protocol_alloc(dcontext, protocol, &text));

    printf("------\n");
    printf("Schema\n");
    printf("------\n\n");
    printf("%s\n", text);

    free(text);
    text = NULL;

    printf("--------\n");
    printf("Messages\n");
    printf("--------\n\n");

    for (i = 0; ; i++)
    {
        status = lwmsg_archive_read_message(archive, &message);

        if (status == LWMSG_STATUS_EOF)
        {
            status = LWMSG_STATUS_SUCCESS;
            break;
        }

        BAIL_ON_ERROR(status);

        BAIL_ON_ERROR(status = lwmsg_assoc_print_message_alloc(lwmsg_archive_as_assoc(archive), &message, &text));
        printf("[%i] %s\n\n", i, text);

        free(text);
        text = NULL;
        lwmsg_archive_destroy_message(archive, &message);
    }

error:

    if (text)
    {
        free(text);
    }

    if (archive)
    {
        lwmsg_archive_destroy_message(archive, &message);
        lwmsg_archive_close(archive);
        lwmsg_archive_delete(archive);
    }

    if (protocol)
    {
        lwmsg_protocol_delete(protocol);
    }

    if (dcontext)
    {
        lwmsg_data_context_delete(dcontext);
    }

    return status;
}

int
main(
    int argc,
    char** argv
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;

    if (!strcmp(argv[1], "dump"))
    {
        BAIL_ON_ERROR(status = archive_dump(argv[2]));
    }

error:

    if (status)
    {
        fprintf(stderr, "Error: %i\n", status);
        return 1;
    }
    else
    {
        return 0;
    }
}
