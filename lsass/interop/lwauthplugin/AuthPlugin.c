#include <CoreServices/CoreServices.h>

#include <Security/AuthorizationPlugin.h>

#include <lwmem.h>
#include <lw/attrs.h>

#include <lsautils.h>
#include <lwstr.h>

#include "AuthPlugin.h"

static OSStatus
AuthPluginDestroy(
        IN AuthorizationPluginRef pPluginRef
        );

static OSStatus
AuthMechanismCreate(
        IN AuthorizationPluginRef pPluginRef,
        IN AuthorizationEngineRef pEngineRef,
        IN AuthorizationMechanismId mechanismId,
        OUT AuthorizationMechanismRef *ppMechanismRef
        );

static OSStatus
AuthMechanismInvoke(
        IN AuthorizationMechanismRef pMechanismRef
        );

static OSStatus
AuthMechanismDeactivate(
        IN AuthorizationMechanismRef pMechanismRef
        );

static OSStatus
AuthMechanismDestroy(
        IN AuthorizationMechanismRef pMechanismRef
        );

static const AuthorizationPluginInterface gPluginInterface = {
    .version             = kAuthorizationPluginInterfaceVersion,
    .PluginDestroy       = AuthPluginDestroy,
    .MechanismCreate     = AuthMechanismCreate,
    .MechanismInvoke     = AuthMechanismInvoke,
    .MechanismDeactivate = AuthMechanismDeactivate,
    .MechanismDestroy    = AuthMechanismDestroy,
};

static PLW_AUTH_MECHANISM gpAuthMechanisms;

static
VOID
LogCallback(
    IN OPTIONAL LW_PVOID Context,
    IN LW_RTL_LOG_LEVEL Level,
    IN OPTIONAL PCSTR ComponentName,
    IN PCSTR FunctionName,
    IN PCSTR FileName,
    IN ULONG LineNumber,
    IN PCSTR Format,
    IN ...
    )
{
    DWORD dwError = 0;
    PSTR formattedMessage = NULL;
    va_list argList;
    PCSTR levelString = NULL;
    size_t messageLength = 0;
    PCSTR optionalNewLine = NULL;

    va_start(argList, Format);
    dwError = LwAllocateStringPrintfV(&formattedMessage, Format, argList);
    va_end(argList);
    if (dwError)
    {
        goto error;
    }

    switch (Level)
    {
        case LW_RTL_LOG_LEVEL_ALWAYS:
            levelString = "ALWAYS";
            break;
        case LW_RTL_LOG_LEVEL_ERROR:
            levelString = "ERROR";
            break;
        case LW_RTL_LOG_LEVEL_WARNING:
            levelString = "WARNING";
            break;
        case LW_RTL_LOG_LEVEL_INFO:
            levelString = "INFO";
            break;
        case LW_RTL_LOG_LEVEL_VERBOSE:
            levelString = "VERBOSE";
            break;
        case LW_RTL_LOG_LEVEL_DEBUG:
            levelString = "DEBUG";
            break;
        case LW_RTL_LOG_LEVEL_TRACE:
            levelString = "TRACE";
            break;
        default:
            levelString = NULL;
            break;
    }

    messageLength = strlen(formattedMessage);
    if (!messageLength || formattedMessage[messageLength-1] != '\n')
    {
        optionalNewLine = "\n";
    }

    printf("%s: [%s() %s:%d] %s%s",
           LW_RTL_LOG_SAFE_STRING(levelString),
           FunctionName,
           FileName,
           LineNumber,
           formattedMessage,
           optionalNewLine);

error:
    if (dwError)
    {
        printf("WARNING: Failed to format log message");
    }

    LW_SAFE_FREE_STRING(formattedMessage);
}

extern OSStatus
AuthorizationPluginCreate(
    const AuthorizationCallbacks        *pAuthCallbacks,
    AuthorizationPluginRef              *ppPlugin,
    const AuthorizationPluginInterface  **ppPluginInterface
    )
{
    LW_AUTH_PLUGIN *pPlugin = NULL;
    OSStatus osStatus = noErr;
    DWORD dwError = LW_ERROR_SUCCESS;

    LwRtlLogSetCallback(LogCallback, NULL);

    dwError = AUTH_PLUGIN_ALLOCATE(pPlugin);
    BAIL_ON_LSA_ERROR(dwError);

    pPlugin->pAuthCallbacks = pAuthCallbacks;

    *ppPlugin = pPlugin;
    *ppPluginInterface = &gPluginInterface;

cleanup:
    return osStatus;

error:
    LW_SAFE_FREE_MEMORY(pPlugin);

    /*
     * The documentation says all errors should return
     * errAuthorizationInternal.
     */
    osStatus = errAuthorizationInternal;
    goto cleanup;
}

static OSStatus
AuthPluginDestroy(
        IN AuthorizationPluginRef pPluginRef
        )
{
    LW_SAFE_FREE_MEMORY(pPluginRef);
    return noErr;
}

VOID
AuthMechanismRegister(
        PLW_AUTH_MECHANISM pAuthMechanism
        )
{
    pAuthMechanism->pNext = gpAuthMechanisms;
    gpAuthMechanisms = pAuthMechanism;
}

static OSStatus
AuthMechanismCreate(
        IN AuthorizationPluginRef pPluginRef,
        IN AuthorizationEngineRef pEngineRef,
        IN AuthorizationMechanismId mechanismId,
        OUT AuthorizationMechanismRef *ppMechanismRef
        )
{
    PLW_AUTH_MECHANISM pAuthMechanism = NULL;
    PLW_AUTH_MECHANISM_INSTANCE pMechanismInstance = NULL;
    OSStatus osStatus = noErr;
    DWORD dwError = LW_ERROR_SUCCESS;

    LSA_LOG_DEBUG("Creating auth mechanism Likewise:%s", mechanismId);

    for (pAuthMechanism = gpAuthMechanisms;
            pAuthMechanism != NULL;
            pAuthMechanism = pAuthMechanism->pNext)
    {
        if (!strcmp(pAuthMechanism->name, mechanismId))
        {
            break;
        }
    }

    if (pAuthMechanism == NULL)
    {
        LSA_LOG_DEBUG("Auth mechanism Likewise:%s not found", mechanismId);
        goto error;
    }

    dwError = AUTH_PLUGIN_ALLOCATE(pMechanismInstance);
    BAIL_ON_LSA_ERROR(dwError);

    pMechanismInstance->pAuthPlugin = pPluginRef;
    pMechanismInstance->pAuthEngine = pEngineRef;
    pMechanismInstance->pAuthMechanism = pAuthMechanism;
    pMechanismInstance->pMechanismData = NULL;

    if (pAuthMechanism->Create)
    {
        if (pAuthMechanism->Create(pMechanismInstance) != noErr)
        {
            goto error;
        }
    }

cleanup:
    *ppMechanismRef = pMechanismInstance;
    return osStatus;

error:
    if (pMechanismInstance)
    {
        if (pMechanismInstance->pAuthMechanism &&
                pMechanismInstance->pAuthMechanism->Destroy)
        {
            pMechanismInstance->pAuthMechanism->Destroy(pMechanismInstance);
        }

        LwFreeMemory(pMechanismInstance);
    }

    /*
     * The documentation says all errors should return
     * errAuthorizationInternal.
     */
    osStatus = errAuthorizationInternal;
    goto cleanup;
}

static OSStatus
AuthMechanismInvoke(
        IN AuthorizationMechanismRef pMechanismRef
        )
{
    PLW_AUTH_MECHANISM_INSTANCE pMechanismInstance = pMechanismRef;
    OSStatus osStatus = noErr;

    if (pMechanismInstance == NULL)
    {
        LSA_LOG_DEBUG(
            "Likewise auth plugin called with NULL mechanism instance");
        goto error;
    }

    if (pMechanismInstance->pAuthMechanism == NULL)
    {
        LSA_LOG_DEBUG(
            "Likewise auth plugin called with NULL mechanism pointer");
        goto error;
    }

    LSA_LOG_DEBUG(
        "Invoking auth mechanism Likewise:%s",
        pMechanismInstance->pAuthMechanism->name);

    if (pMechanismInstance->pAuthMechanism->Invoke != NULL &&
                pMechanismInstance->pAuthMechanism->Invoke(
                    pMechanismInstance) != noErr)
    {
        LSA_LOG_DEBUG(
            "Invoking auth mechanism Likewise:%s failed",
            pMechanismInstance->pAuthMechanism->name);
        goto error;
    }

cleanup:
    return osStatus;

error:
    /*
     * The documentation says all errors should return
     * errAuthorizationInternal.
     */
    osStatus = errAuthorizationInternal;
    goto cleanup;
}

static OSStatus
AuthMechanismDeactivate(
        IN AuthorizationMechanismRef pMechanismRef
        )
{
    PLW_AUTH_MECHANISM_INSTANCE pMechanismInstance = pMechanismRef;
    OSStatus osStatus = noErr;

    if (pMechanismInstance == NULL)
    {
        LSA_LOG_DEBUG(
            "Likewise auth plugin called with NULL mechanism instance");
        goto error;
    }

    if (pMechanismInstance->pAuthMechanism == NULL)
    {
        LSA_LOG_DEBUG(
            "Likewise auth plugin called with NULL mechanism pointer");
        goto error;
    }

    LSA_LOG_DEBUG(
        "Deactivating auth mechanism Likewise:%s",
        pMechanismInstance->pAuthMechanism->name);

    if (pMechanismInstance->pAuthMechanism->Deactivate != NULL &&
                pMechanismInstance->pAuthMechanism->Deactivate(
                    pMechanismInstance) != noErr)
    {
        LSA_LOG_DEBUG(
            "Deactivating auth mechanism Likewise:%s failed",
            pMechanismInstance->pAuthMechanism->name);
        goto error;
    }

cleanup:
    return osStatus;

error:
    /*
     * The documentation says all errors should return
     * errAuthorizationInternal.
     */
    osStatus = errAuthorizationInternal;
    goto cleanup;
}

static OSStatus
AuthMechanismDestroy(
        IN AuthorizationMechanismRef pMechanismRef
        )
{
    PLW_AUTH_MECHANISM_INSTANCE pMechanismInstance = pMechanismRef;
    OSStatus osStatus = noErr;

    if (pMechanismInstance == NULL)
    {
        LSA_LOG_DEBUG(
            "Likewise auth plugin called with NULL mechanism instance");
        goto error;
    }

    if (pMechanismInstance->pAuthMechanism == NULL)
    {
        LSA_LOG_DEBUG(
            "Likewise auth plugin called with NULL mechanism pointer");
        goto error;
    }

    LSA_LOG_DEBUG(
        "Destroying auth mechanism Likewise:%s",
        pMechanismInstance->pAuthMechanism->name);

    if (pMechanismInstance->pAuthMechanism->Destroy != NULL &&
                pMechanismInstance->pAuthMechanism->Destroy(
                    pMechanismInstance) != noErr)
    {
        LSA_LOG_DEBUG(
            "Destroying auth mechanism Likewise:%s failed",
            pMechanismInstance->pAuthMechanism->name);
        goto error;
    }

cleanup:
    return osStatus;

error:
    /*
     * The documentation says all errors should return
     * errAuthorizationInternal.
     */
    osStatus = errAuthorizationInternal;
    goto cleanup;
}
