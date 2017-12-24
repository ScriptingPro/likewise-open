/*
 * Copyright (C) VMware. All rights reserved.
 */

#include "includes.h"

static
DWORD
VmDirLdapInitializeWithKerberos(
    PCSTR            pszURI,
    PCSTR            pszUPN,
    PCSTR            pszPassword,
    PCSTR            pszCachePath,
    LDAP**           ppLd
    );

static
DWORD
VmDirLdapInitializeWithSRP(
    PCSTR            pszURI,
    PCSTR            pszUPN,
    PCSTR            pszPassword,
    PCSTR            pszCachePath,
    LDAP**           ppLd
    );

static
int
VmDirSASLInteractionKerberos(
    LDAP*    pLd,
    unsigned flags,
    PVOID    pDefaults,
    PVOID    pIn
    );

static
int
VmDirSASLInteractionSRP(
     LDAP*    pLd,
     unsigned flags,
     PVOID    pDefaults,
     PVOID    pIn
     );

static
DWORD
VmDirLdapGetDN(
	LDAP*        pLd,
	LDAPMessage* pMessage,
	PCSTR        pszAttrName,
	PSTR*        ppszDN,
	BOOLEAN      bOptional
	);

static
DWORD
VmDirLdapGetInt32Value(
	LDAP*        pLd,
	LDAPMessage* pMessage,
	PCSTR        pszAttrName,
	PINT32       pValue,
	BOOLEAN      bOptional
	);

static
DWORD
VmDirLdapGetUint32Value(
	LDAP*        pLd,
	LDAPMessage* pMessage,
	PCSTR        pszAttrName,
	PUINT32      pValue,
	BOOLEAN      bOptional
	);

static
DWORD
VmDirLdapGetInt64Value(
	LDAP*        pLd,
	LDAPMessage* pMessage,
	PCSTR        pszAttrName,
	PINT64       pValue,
	BOOLEAN      bOptional
	);

static
DWORD
VmDirLdapGetUint64Value(
	LDAP*        pLd,
	LDAPMessage* pMessage,
	PCSTR        pszAttrName,
	PUINT64      pValue,
	BOOLEAN      bOptional
	);

static
DWORD
VmDirLdapGetOptionalStringValue(
	LDAP*        pLd,
	LDAPMessage* pMessage,
	PCSTR        pszAttrName,
	PSTR*        ppszValue
	);

static
DWORD
VmDirLdapGetStringValue(
	LDAP*        pLd,
	LDAPMessage* pMessage,
	PCSTR        pszAttrName,
	PSTR*        ppszValue
	);

static
DWORD
VmDirLdapGetStringArray(
	LDAP*        pLd,
	LDAPMessage* pMessage,
	PCSTR        pszAttrName,
	BOOLEAN      bOptional,
	PSTR**       pppszStrArray,
	PDWORD       pdwCount
	);

DWORD
VmDirLdapInitialize(
	PCSTR            pszURI,
	PCSTR            pszUPN,
	PCSTR            pszPassword,
    PCSTR            pszCachePath,
	LDAP**           ppLd
	)
{
    DWORD dwError = 0;
    
    switch (gVmdirGlobals.bindProtocol)
    {
        case VMDIR_BIND_PROTOCOL_KERBEROS:
            
            dwError = VmDirLdapInitializeWithKerberos(
                            pszURI,
                            pszUPN,
                            pszPassword,
                            pszCachePath,
                            ppLd);
            
            break;
            
        case VMDIR_BIND_PROTOCOL_SRP:
            
            dwError = VmDirLdapInitializeWithSRP(
                            pszURI,
                            pszUPN,
                            pszPassword,
                            pszCachePath,
                            ppLd);
            
            break;
            
        default:
            
            dwError = ERROR_INVALID_STATE;
            
            break;
    }
    
    return dwError;
}

DWORD
VmDirLdapQuerySingleObject(
	LDAP*         pLd,
	PCSTR         pszBaseDN,
	int           scope,
	PCSTR         pszFilter,
	char**        attrs,
	LDAPMessage** ppMessage
	)
{
	DWORD dwError = 0;
	DWORD dwNumObjects = 0;
	LDAPMessage* pMessage = NULL;

	dwError = VmDirLdapQueryObjects(
					pLd,
					pszBaseDN,
					scope,
					pszFilter,
					attrs,
					-1,
					&pMessage);
	BAIL_ON_VMDIRDB_ERROR(dwError);

	dwNumObjects = ldap_count_entries(pLd, pMessage);

	if (dwNumObjects == 0)
	{
		dwError = LW_ERROR_NO_SUCH_OBJECT;
	}
	else if (dwNumObjects != 1)
	{
		dwError = ERROR_INVALID_DATA;
	}
	BAIL_ON_VMDIRDB_ERROR(dwError);

	*ppMessage = pMessage;

cleanup:

	return dwError;

error:

	*ppMessage = NULL;

	if (pMessage)
	{
		VmDirLdapFreeMessage(pMessage);
	}

	goto cleanup;
}

DWORD
VmDirLdapQueryObjects(
	LDAP*         pLd,
	PCSTR         pszBaseDN,
	int           scope,
	PCSTR         pszFilter,
	char**        attrs,
	int           sizeLimit,
	LDAPMessage** ppMessage
	)
{
	DWORD dwError = 0;

	struct timeval waitTime = {0};

	waitTime.tv_sec  = DEFAULT_LDAP_QUERY_TIMEOUT_SECS;
	waitTime.tv_usec = 0;

	dwError = LwMapLdapErrorToLwError(
				ldap_search_ext_s(
					pLd,
					pszBaseDN,
					scope,
					pszFilter,
					attrs,
					FALSE,     /* Attrs only      */
					NULL,      /* Server controls */
					NULL,      /* Client controls */
					&waitTime,
					sizeLimit, /* size limit      */
					ppMessage));

	return dwError;
}

DWORD
VmDirLdapGetValues(
	LDAP*        pLd,
	LDAPMessage* pMessage,
	PVMDIR_ATTR  pValueArray,
	DWORD        dwNumValues
	)
{
	DWORD dwError = 0;
	DWORD iValue = 0;

	for (; iValue < dwNumValues; iValue++)
	{
		PVMDIR_ATTR pAttr = &pValueArray[iValue];

		switch (pAttr->type)
		{
			case VMDIR_ATTR_TYPE_DN:

				dwError = VmDirLdapGetDN(
								pLd,
								pMessage,
								pAttr->pszName,
								pAttr->dataRef.ppszData,
								pAttr->bOptional);

				break;

			case VMDIR_ATTR_TYPE_STRING:

				if (pAttr->bOptional)
				{
					dwError = VmDirLdapGetOptionalStringValue(
									pLd,
									pMessage,
									pAttr->pszName,
									pAttr->dataRef.ppszData);
				}
				else
				{
					dwError = VmDirLdapGetStringValue(
								pLd,
								pMessage,
								pAttr->pszName,
								pAttr->dataRef.ppszData);
				}

				break;

			case VMDIR_ATTR_TYPE_INT32:

				if (pAttr->size < sizeof(INT32))
				{
					dwError = ERROR_INVALID_PARAMETER;
					BAIL_ON_VMDIRDB_ERROR(dwError);
				}

				dwError = VmDirLdapGetInt32Value(
								pLd,
								pMessage,
								pAttr->pszName,
								pAttr->dataRef.pData_int32,
								pAttr->bOptional);

				break;

			case VMDIR_ATTR_TYPE_UINT32:

				if (pAttr->size < sizeof(UINT32))
				{
					dwError = ERROR_INVALID_PARAMETER;
					BAIL_ON_VMDIRDB_ERROR(dwError);
				}

				dwError = VmDirLdapGetUint32Value(
								pLd,
								pMessage,
								pAttr->pszName,
								pAttr->dataRef.pData_uint32,
								pAttr->bOptional);

				break;

			case VMDIR_ATTR_TYPE_INT64:

				if (pAttr->size < sizeof(INT64))
				{
					dwError = ERROR_INVALID_PARAMETER;
					BAIL_ON_VMDIRDB_ERROR(dwError);
				}

				dwError = VmDirLdapGetInt64Value(
								pLd,
								pMessage,
								pAttr->pszName,
								pAttr->dataRef.pData_int64,
								pAttr->bOptional);

				break;

			case VMDIR_ATTR_TYPE_UINT64:

				if (pAttr->size < sizeof(UINT64))
				{
					dwError = ERROR_INVALID_PARAMETER;
					BAIL_ON_VMDIRDB_ERROR(dwError);
				}

				dwError = VmDirLdapGetUint64Value(
								pLd,
								pMessage,
								pAttr->pszName,
								pAttr->dataRef.pData_uint64,
								pAttr->bOptional);

				break;

			case VMDIR_ATTR_TYPE_MULTI_STRING:

				dwError = VmDirLdapGetStringArray(
								pLd,
								pMessage,
								pAttr->pszName,
								pAttr->bOptional,
								pAttr->dataRef.pppszStrArray,
								pAttr->pdwCount);
				BAIL_ON_VMDIRDB_ERROR(dwError);

				break;

			default:

				dwError = ERROR_INVALID_PARAMETER;
				BAIL_ON_VMDIRDB_ERROR(dwError);

				break;
		}
	}

error:

	return dwError;
}

VOID
VmDirLdapFreeMessage(
	LDAPMessage* pMessage
	)
{
	ldap_msgfree(pMessage);
}

VOID
VmDirLdapClose(
	LDAP* pLd
	)
{
	ldap_unbind_ext(pLd, NULL, NULL);
}

static
DWORD
VmDirLdapInitializeWithKerberos(
    PCSTR            pszURI,
    PCSTR            pszUPN,
    PCSTR            pszPassword,
    PCSTR            pszCachePath,
    LDAP**           ppLd
    )
{
    DWORD dwError = 0;
    const int ldapVer = LDAP_VERSION3;
    PSTR  pszUPN_local = NULL;
    LDAP* pLd = NULL;
    PSTR pszOldCachePath = NULL;
    
    dwError = LwMapLdapErrorToLwError(
                    ldap_initialize(&pLd, pszURI));
    BAIL_ON_VMDIRDB_ERROR(dwError);
    
    dwError = LwMapLdapErrorToLwError(
                    ldap_set_option(pLd, LDAP_OPT_PROTOCOL_VERSION, &ldapVer));
    BAIL_ON_VMDIRDB_ERROR(dwError);
    
    dwError = LwMapLdapErrorToLwError(
                    ldap_set_option(
                                  pLd,
                                  LDAP_OPT_X_SASL_NOCANON,
                                  LDAP_OPT_ON));
    BAIL_ON_VMDIRDB_ERROR(dwError);
    
    dwError = LwKrb5SetThreadDefaultCachePath(
                    pszCachePath,
                    &pszOldCachePath);
    BAIL_ON_VMDIRDB_ERROR(dwError);
    
    dwError = LwMapLdapErrorToLwError(
                    ldap_sasl_interactive_bind_s(
                                               pLd,
                                               NULL,
                                               "GSSAPI",
                                               NULL,
                                               NULL,
                                               LDAP_SASL_QUIET,
                                               &VmDirSASLInteractionKerberos,
                                               NULL));
    BAIL_ON_VMDIRDB_ERROR(dwError);
    
    *ppLd = pLd;
    
cleanup:
    
    if (pszOldCachePath)
    {
        LwKrb5SetThreadDefaultCachePath(
                pszOldCachePath,
                NULL);
        LwFreeString(pszOldCachePath);
    }
    
    LW_SAFE_FREE_STRING(pszUPN_local);
    
    return dwError;
    
error:
    
    *ppLd = NULL;
    
    if (pLd)
    {
        VmDirLdapClose(pLd);
    }
    
    goto cleanup;
}

static
DWORD
VmDirLdapInitializeWithSRP(
   PCSTR            pszURI,
   PCSTR            pszUPN,
   PCSTR            pszPassword,
   PCSTR            pszCachePath,
   LDAP**           ppLd
   )
{
    DWORD dwError = 0;
    const int ldapVer = LDAP_VERSION3;
    VMDIR_SASL_INFO srpDefault = {0};
    PSTR  pszUPN_local = NULL;
    LDAP* pLd = NULL;
    
    dwError = LwMapLdapErrorToLwError(
                  ldap_initialize(&pLd, pszURI));
    BAIL_ON_VMDIRDB_ERROR(dwError);
    
    dwError = LwMapLdapErrorToLwError(
                  ldap_set_option(pLd, LDAP_OPT_PROTOCOL_VERSION, &ldapVer));
    BAIL_ON_VMDIRDB_ERROR(dwError);
    
    dwError = LwMapLdapErrorToLwError(
                  ldap_set_option(
                                  pLd,
                                  LDAP_OPT_X_SASL_NOCANON,
                                  LDAP_OPT_ON));
    BAIL_ON_VMDIRDB_ERROR(dwError);
    
    dwError = LwAllocateString(pszUPN, &pszUPN_local);
    BAIL_ON_VMDIRDB_ERROR(dwError);
    
    LwStrToLower(pszUPN_local);
    
    srpDefault.pszAuthName = pszUPN_local;
    
    srpDefault.pszPassword = pszPassword;
    
    dwError = LwMapLdapErrorToLwError(
                  ldap_sasl_interactive_bind_s(
                                               pLd,
                                               NULL,
                                               "SRP",
                                               NULL,
                                               NULL,
                                               LDAP_SASL_QUIET,
                                               &VmDirSASLInteractionSRP,
                                               &srpDefault));
    BAIL_ON_VMDIRDB_ERROR(dwError);
    
    *ppLd = pLd;
    
cleanup:
    
    LW_SAFE_FREE_STRING(pszUPN_local);
    
    return dwError;
    
error:
    
    *ppLd = NULL;
    
    if (pLd)
    {
        VmDirLdapClose(pLd);
    }
    
    goto cleanup;
}

static
int
VmDirSASLInteractionKerberos(
    LDAP *      pLd,
    unsigned    flags,
    void *      pDefaults,
    void *      pIn
    )
{
    // dummy function to satisfy ldap_sasl_interactive_bind call
    return LDAP_SUCCESS;
}

static
int
VmDirSASLInteractionSRP(
    LDAP *      pLd,
    unsigned    flags,
    void *      pDefaults,
    void *      pIn
    )
{
    sasl_interact_t* pInteract = pIn;
    PVMDIR_SASL_INFO pDef = pDefaults;
    
    while( (pDef != NULL) && (pInteract->id != SASL_CB_LIST_END) )
    {
        switch( pInteract->id )
        {
            case SASL_CB_GETREALM:
                pInteract->defresult = pDef->pszRealm;
                break;
            case SASL_CB_AUTHNAME:
                pInteract->defresult = pDef->pszAuthName;
                break;
            case SASL_CB_PASS:
                pInteract->defresult = pDef->pszPassword;
                break;
            case SASL_CB_USER:
                pInteract->defresult = pDef->pszUser;
                break;
            default:
                break;
        }
        
        pInteract->result = (pInteract->defresult) ? pInteract->defresult : "";
        pInteract->len    = strlen( pInteract->result );
        
        pInteract++;
    }
    
    return LDAP_SUCCESS;
}

static
DWORD
VmDirLdapGetDN(
	LDAP*        pLd,
	LDAPMessage* pMessage,
	PCSTR        pszAttrName,
	PSTR*        ppszDN,
	BOOLEAN      bOptional
	)
{
	DWORD dwError = 0;
	PSTR  pszDN_ldap = NULL;
	PSTR  pszDN = NULL;
	PSTR  pszDNRef = NULL;

	pszDN_ldap = ldap_get_dn(pLd, pMessage);
	if (IsNullOrEmptyString(pszDN_ldap))
	{
		if (!bOptional)
		{
			dwError = LW_ERROR_NO_ATTRIBUTE_VALUE;
			BAIL_ON_VMDIRDB_ERROR(dwError);
		}
		else
		{
			pszDNRef = "";
		}
	}
	else
	{
		pszDNRef = pszDN_ldap;
	}

	dwError = LwAllocateString(pszDNRef, &pszDN);
	BAIL_ON_VMDIRDB_ERROR(dwError);

	*ppszDN = pszDN;

cleanup:

	if (pszDN_ldap)
	{
		ldap_memfree(pszDN_ldap);
	}

	return dwError;

error:

	*ppszDN = NULL;

	goto cleanup;
}

static
DWORD
VmDirLdapGetInt32Value(
	LDAP*        pLd,
	LDAPMessage* pMessage,
	PCSTR        pszAttrName,
	PINT32       pValue,
	BOOLEAN      bOptional
	)
{
	DWORD dwError = 0;
	PSTR  pszValue = NULL;
	PSTR  pszValueRef = NULL;

	if (bOptional)
	{
		dwError = VmDirLdapGetOptionalStringValue(
						pLd,
						pMessage,
						pszAttrName,
						&pszValue);
		BAIL_ON_VMDIRDB_ERROR(dwError);

		pszValueRef = !pszValue ? "0" : pszValue;
	}
	else
	{
		dwError = VmDirLdapGetStringValue(
						pLd,
						pMessage,
						pszAttrName,
						&pszValue);
		BAIL_ON_VMDIRDB_ERROR(dwError);

		pszValueRef = pszValue;
	}

	*pValue = atoi(pszValueRef);

cleanup:

	LW_SAFE_FREE_MEMORY(pszValue);

	return dwError;

error:

	*pValue = 0;

	goto cleanup;
}

static
DWORD
VmDirLdapGetUint32Value(
	LDAP*        pLd,
	LDAPMessage* pMessage,
	PCSTR        pszAttrName,
	PUINT32      pValue,
	BOOLEAN      bOptional
	)
{
	DWORD dwError = 0;
	PSTR  pszValue = NULL;
	PSTR  pszValueRef = NULL;

	if (bOptional)
	{
		dwError = VmDirLdapGetOptionalStringValue(
							pLd,
							pMessage,
							pszAttrName,
							&pszValue);
		BAIL_ON_VMDIRDB_ERROR(dwError);

		pszValueRef = !pszValue ? "0" : pszValue;
	}
	else
	{
		dwError = VmDirLdapGetStringValue(
							pLd,
							pMessage,
							pszAttrName,
							&pszValue);
		BAIL_ON_VMDIRDB_ERROR(dwError);

		pszValueRef = pszValue;
	}

	*pValue = atoi(pszValueRef);

cleanup:

	LW_SAFE_FREE_MEMORY(pszValue);

	return dwError;

error:

	*pValue = 0;

	goto cleanup;
}

static
DWORD
VmDirLdapGetInt64Value(
	LDAP*        pLd,
	LDAPMessage* pMessage,
	PCSTR        pszAttrName,
	PINT64       pValue,
	BOOLEAN      bOptional
	)
{
	DWORD dwError = 0;
	PSTR  pszValue = NULL;
	PSTR  pszValueRef = NULL;
	PSTR  pszEnd = NULL;
	INT64 val = 0;

	if (bOptional)
	{
		dwError = VmDirLdapGetOptionalStringValue(
						pLd,
						pMessage,
						pszAttrName,
						&pszValue);
		BAIL_ON_VMDIRDB_ERROR(dwError);

		pszValueRef = !pszValue ? "0" : pszValue;
	}
	else
	{
		dwError = VmDirLdapGetStringValue(
						pLd,
						pMessage,
						pszAttrName,
						&pszValue);
		BAIL_ON_VMDIRDB_ERROR(dwError);

		pszValueRef = pszValue;
	}

	val = strtoll(pszValueRef, &pszEnd, 10);

	if (!pszEnd || (pszEnd == pszValueRef) || (*pszEnd != '\0'))
	{
		dwError = ERROR_INVALID_DATA;
		BAIL_ON_VMDIRDB_ERROR(dwError);
	}

	*pValue = val;

cleanup:

	LW_SAFE_FREE_MEMORY(pszValue);

	return dwError;

error:

	*pValue = 0;

	goto cleanup;
}

static
DWORD
VmDirLdapGetUint64Value(
	LDAP*        pLd,
	LDAPMessage* pMessage,
	PCSTR        pszAttrName,
	PUINT64      pValue,
	BOOLEAN      bOptional
	)
{
	DWORD  dwError = 0;
	PSTR   pszValue = NULL;
	PSTR   pszValueRef = NULL;
	PSTR   pszEnd = NULL;
	UINT64 val = 0;

	if (bOptional)
	{
		dwError = VmDirLdapGetOptionalStringValue(
						pLd,
						pMessage,
						pszAttrName,
						&pszValue);
		BAIL_ON_VMDIRDB_ERROR(dwError);

		pszValueRef = !pszValue ? "0" : pszValue;
	}
	else
	{
		dwError = VmDirLdapGetStringValue(
						pLd,
						pMessage,
						pszAttrName,
						&pszValue);
		BAIL_ON_VMDIRDB_ERROR(dwError);

		pszValueRef = pszValue;
	}

	val = strtoull(pszValueRef, &pszEnd, 10);

	if (!pszEnd || (pszEnd == pszValueRef) || (*pszEnd != '\0'))
	{
		dwError = ERROR_INVALID_DATA;
		BAIL_ON_VMDIRDB_ERROR(dwError);
	}

	*pValue = val;

cleanup:

	LW_SAFE_FREE_MEMORY(pszValue);

	return dwError;

error:

	*pValue = 0;

	goto cleanup;
}

static
DWORD
VmDirLdapGetOptionalStringValue(
	LDAP*        pLd,
	LDAPMessage* pMessage,
	PCSTR        pszAttrName,
	PSTR*        ppszValue
	)
{
	DWORD dwError = 0;
	PSTR  pszValue = NULL;

	dwError = VmDirLdapGetStringValue(
					pLd,
					pMessage,
					pszAttrName,
					&pszValue);
	BAIL_ON_VMDIRDB_ERROR(dwError);

	*ppszValue = pszValue;

cleanup:

	return dwError;

error:

	*ppszValue = NULL;

	if (dwError == LW_ERROR_NO_ATTRIBUTE_VALUE)
	{
		dwError = LW_ERROR_SUCCESS;
	}

	goto cleanup;
}

static
DWORD
VmDirLdapGetStringValue(
	LDAP*        pLd,
	LDAPMessage* pMessage,
	PCSTR        pszAttrName,
	PSTR*        ppszValue
	)
{
	DWORD dwError = 0;
	PSTR* ppszValues = NULL;
	PSTR  pszValue = NULL;

	ppszValues = (PSTR*)ldap_get_values(pLd, pMessage, pszAttrName);
	if (!ppszValues || !*ppszValues)
	{
		dwError = LW_ERROR_NO_ATTRIBUTE_VALUE;
		BAIL_ON_VMDIRDB_ERROR(dwError);
	}

	dwError = LwAllocateString(*ppszValues, &pszValue);
	BAIL_ON_VMDIRDB_ERROR(dwError);

	*ppszValue = pszValue;

cleanup:

	if (ppszValues)
	{
		ldap_value_free(ppszValues);
	}

	return dwError;

error:

	*ppszValue = NULL;

	goto cleanup;
}

static
DWORD
VmDirLdapGetStringArray(
	LDAP*        pLd,
	LDAPMessage* pMessage,
	PCSTR        pszAttrName,
	BOOLEAN      bOptional,
	PSTR**       pppszStrArray,
	PDWORD       pdwCount
	)
{
	DWORD dwError = 0;
	PSTR* ppszLDAPValues = NULL;
	PSTR* ppszStrArray = NULL;
	DWORD dwCount = 0;

	ppszLDAPValues = (PSTR*)ldap_get_values(pLd, pMessage, pszAttrName);
	if (!ppszLDAPValues || !*ppszLDAPValues)
	{
		if (!bOptional)
		{
			dwError = LW_ERROR_NO_ATTRIBUTE_VALUE;
			BAIL_ON_VMDIRDB_ERROR(dwError);
		}
	}
	else
	{
		DWORD iValue = 0;

		dwCount = ldap_count_values(ppszLDAPValues);

		dwError = LwAllocateMemory(
                              sizeof(PSTR) * dwCount,
                              (PVOID*)&ppszStrArray);
		BAIL_ON_VMDIRDB_ERROR(dwError);

		for (; iValue < dwCount; iValue++)
		{
			PSTR pszValue = ppszLDAPValues[iValue];

			dwError = LwAllocateString(pszValue, &ppszStrArray[iValue]);
			BAIL_ON_VMDIRDB_ERROR(dwError);
		}
	}

	*pppszStrArray = ppszStrArray;
	*pdwCount      = dwCount;

cleanup:

	return dwError;

error:

	*pppszStrArray = NULL;
	*pdwCount = 0;

	if (ppszStrArray)
	{
		LwFreeStringArray(ppszStrArray, dwCount);
	}

	goto cleanup;
}

#if 0 /* TBD:Adam-Reference */
typedef struct _VMDIRDB_LDAPQUERY_MAP_ENTRY
{
    PSTR pszSqlQuery;
    PSTR pszLdapQuery;
    ULONG uScope;
} VMDIRDB_LDAPQUERY_MAP_ENTRY, *PVMDIRDB_LDAPQUERY_MAP_ENTRY;

typedef struct _VMDIRDB_LDAPQUERY_MAP
{
    DWORD dwNumEntries;
    VMDIRDB_LDAPQUERY_MAP_ENTRY queryMap[];
} VMDIRDB_LDAPQUERY_MAP;

    PSTR ppszBase[] = {"cn=builtin,dc=lightwave,dc=local", 0};
#endif /* if 0 */

static DWORD
VmDirAllocLdapQueryMapEntry(
    PSTR pszSql,
    PSTR pszLdapBasePrefix,
    PSTR pszLdapBase,
    PSTR pszLdapFilter,
    ULONG uScope,
    DWORD dwIndex,
    PVMDIRDB_LDAPQUERY_MAP pLdapMap)
{
    DWORD dwError = 0;
    PSTR pszBaseDn = NULL;
    PSTR pszBaseDnAlloc = NULL;

    if (!pLdapMap || dwIndex > pLdapMap->dwMaxEntries)
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIRDB_ERROR(dwError);
    }

    if (pszLdapBasePrefix)
    {
        dwError = LwAllocateStringPrintf(
                    &pszBaseDnAlloc,
                    "%s,%s",
                    pszLdapBasePrefix,
                    pszLdapBase);
        BAIL_ON_VMDIRDB_ERROR(dwError);
        pszBaseDn = pszBaseDnAlloc;
    }
    else
    {
        pszBaseDn = pszLdapBase;
    }

    pLdapMap->queryMap[dwIndex].uScope = uScope;
    dwError = LwAllocateString(
                  pszSql,
                  (VOID *) &pLdapMap->queryMap[dwIndex].pszSqlQuery);
    BAIL_ON_VMDIRDB_ERROR(dwError);
    dwError = LwAllocateString(
                  pszLdapFilter,
                  (VOID *) &pLdapMap->queryMap[dwIndex].pszLdapQuery);
    BAIL_ON_VMDIRDB_ERROR(dwError);
    dwError = LwAllocateString(
                  pszBaseDn,
                  (VOID *) &pLdapMap->queryMap[dwIndex].pszLdapBase);
    BAIL_ON_VMDIRDB_ERROR(dwError);

cleanup:
    LW_SAFE_FREE_STRING(pszBaseDnAlloc);
    return dwError;

error:
    LW_SAFE_FREE_MEMORY(pLdapMap->queryMap[dwIndex].pszSqlQuery);
    LW_SAFE_FREE_MEMORY(pLdapMap->queryMap[dwIndex].pszLdapQuery);
    LW_SAFE_FREE_MEMORY(pLdapMap->queryMap[dwIndex].pszLdapBase);
    goto cleanup;
}

DWORD
VmDirAllocLdapQueryMap(
    PSTR pszSearchBase,
    PVMDIRDB_LDAPQUERY_MAP *ppLdapMap)
{
    DWORD dwError = 0;
    DWORD dwMaxEntries = 64; /* TBD, could be much larger */
    DWORD i = 0;
    PVMDIRDB_LDAPQUERY_MAP pLdapMap = NULL;

    dwError = LwAllocateMemory(
                  sizeof(VMDIRDB_LDAPQUERY_MAP) +
                      sizeof(VMDIRDB_LDAPQUERY_MAP_ENTRY) * dwMaxEntries,
                  (VOID *) &pLdapMap);
    BAIL_ON_VMDIRDB_ERROR(dwError);
    pLdapMap->dwMaxEntries = dwMaxEntries;

#if 0 /* TBD: Don't need to store pszSearchBase in map context */
    dwError = LwAllocateString(
                  pszSearchBase,
                  (VOID *) &pLdapMap->pszSearchBase);
    BAIL_ON_VMDIRDB_ERROR(dwError);
#endif

    /* Initialize map entries */
    dwError = VmDirAllocLdapQueryMapEntry(
                  "ObjectClass = 5",
                  NULL,                  /* SearchBasePrefix (optional) */
                  pszSearchBase,
                  "(cn=*)",
                  LDAP_SCOPE_SUBTREE,
                  i++,
                  pLdapMap);
    BAIL_ON_VMDIRDB_ERROR(dwError);
   
    /* Initialize filters */

    /* LSAR filters */

    /* SAMR filters */
    dwError = VmDirAllocLdapQueryMapEntry(
                  "ObjectClass=1 OR ObjectClass=2",
                  "cn=builtin",          /* SearchBasePrefix (optional) */
                  pszSearchBase,
                  "(cn=*)",
                  LDAP_SCOPE_SUBTREE,
                  i++,
                  pLdapMap);
    BAIL_ON_VMDIRDB_ERROR(dwError);

    pLdapMap->dwNumEntries = i;
    *ppLdapMap = pLdapMap;

cleanup:
    return dwError;

error:
    VmDirFreeLdapQueryMap(&pLdapMap);
    goto cleanup;
}


DWORD
VmDirGetFilterLdapQueryMap(
    PSTR pszSql,
    PSTR *ppszSearchBase, /* Do not free, this is an alias */
    PSTR *ppszLdapFilter, /* Do not free, this is an alias */
    DWORD *puScope) /* Do not free, this is an alias */
{
    DWORD dwError = 0;
    DWORD dwIndex = 0;
    DWORD bFound = 0;
    PVMDIRDB_LDAPQUERY_MAP pLdapMap = gVmdirGlobals.pLdapMap;

    /* This will probably have to be a "fuzzy" search */
    for (dwIndex = 0; dwIndex < pLdapMap->dwNumEntries; dwIndex++)
    {
        if (!strcasecmp(pszSql, pLdapMap->queryMap[dwIndex].pszSqlQuery))
        {
            bFound = 1;
            break;
        }
    }
    if (bFound)
    {
        *ppszSearchBase = pLdapMap->queryMap[dwIndex].pszLdapBase;
        *ppszLdapFilter = pLdapMap->queryMap[dwIndex].pszLdapQuery;
        *puScope = pLdapMap->queryMap[dwIndex].uScope;
    }
    else
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIRDB_ERROR(dwError);
    }

cleanup:
    return dwError;

error:
    goto cleanup;
}

DWORD
VmDirFreeLdapQueryMap(
    PVMDIRDB_LDAPQUERY_MAP *ppLdapMap)
{
    DWORD dwError = 0;
    DWORD dwIndex = 0;
    PVMDIRDB_LDAPQUERY_MAP pLdapMap = NULL;

    if (!ppLdapMap)
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIRDB_ERROR(dwError);
    }
    pLdapMap = *ppLdapMap;

    for (dwIndex=0; dwIndex < pLdapMap->dwNumEntries; dwIndex++)
    {
        LW_SAFE_FREE_MEMORY(pLdapMap->queryMap[dwIndex].pszSqlQuery);
        LW_SAFE_FREE_MEMORY(pLdapMap->queryMap[dwIndex].pszLdapQuery);
        LW_SAFE_FREE_MEMORY(pLdapMap->queryMap[dwIndex].pszLdapBase);
    }
    
#if 0
    LW_SAFE_FREE_MEMORY(pLdapMap->pszSearchBase);
#endif
    LW_SAFE_FREE_MEMORY(pLdapMap);
    *ppLdapMap = NULL;

cleanup:
    return dwError;

error:
    goto cleanup;
}

#if 0
typedef struct _VMDIRDB_LDAPATTR_MAP_ENTRY
{
    PWSTR pwszAttribute;
    PSTR pszAttribute;
} VMDIRDB_LDAPATTR_MAP_ENTRY, *PVMDIRDB_LDAPATTR_MAP_ENTRY;

typedef struct _VMDIRDB_LDAPATTR_MAP
{
    DWORD dwNumEntries;
    DWORD dwMaxEntries;
    VMDIRDB_LDAPATTR_MAP_ENTRY attrMap[];
} VMDIRDB_LDAPATTR_MAP, *PVMDIRDB_LDAPATTR_MAP;
#endif

#define wszVMDIR_DB_DIR_ATTR_EOL NULL
   

DWORD
VmDirAllocLdapAttributeMap(
    PVMDIRDB_LDAPATTR_MAP *ppAttrMap)
{
    DWORD dwError = 0;
    DWORD dwMaxEntries = 0;
    DWORD i = 0;
    PVMDIRDB_LDAPATTR_MAP pAttrMap = NULL;

    WCHAR wszVMDIR_DB_DIR_ATTR_COMMON_NAME[] = VMDIR_DB_DIR_ATTR_COMMON_NAME;
    WCHAR wszVMDIR_DB_DIR_ATTR_DISTINGUISHED_NAME[] = VMDIR_DB_DIR_ATTR_DISTINGUISHED_NAME;
    WCHAR wszVMDIR_DB_DIR_ATTR_SAM_ACCOUNT_NAME[] = VMDIR_DB_DIR_ATTR_SAM_ACCOUNT_NAME;
    WCHAR wszVMDIR_DB_DIR_ATTR_SECURITY_DESCRIPTOR[] = VMDIR_DB_DIR_ATTR_SECURITY_DESCRIPTOR;
    WCHAR wszVMDIR_DB_DIR_ATTR_OBJECT_CLASS[] = VMDIR_DB_DIR_ATTR_OBJECT_CLASS;
    WCHAR wszVMDIR_DB_DIR_ATTR_CREATED_TIME[] = VMDIR_DB_DIR_ATTR_CREATED_TIME;
    WCHAR wszVMDIR_DB_DIR_ATTR_OBJECT_SID[] = VMDIR_DB_DIR_ATTR_OBJECT_SID;
    WCHAR wszVMDIR_DB_DIR_ATTR_UID[] = VMDIR_DB_DIR_ATTR_UID;
    WCHAR wszVMDIR_DB_DIR_ATTR_MEMBERS[] = VMDIR_DB_DIR_ATTR_MEMBERS;

    /* The order of szAttributes and wszAttributes MUST be the same!!! */
    CHAR *szAttributes[] = {
        "cn",
        "dn",
        "sAMAccountName",
        "nTSecurityDescriptor",
        "objectClass",
        "createTimeStamp",
        "objectSid",
        "uid", /* not stored in vmdird?? */
        "objectSid",
    };

    WCHAR *wszAttributes[] = {
        wszVMDIR_DB_DIR_ATTR_COMMON_NAME,
        wszVMDIR_DB_DIR_ATTR_DISTINGUISHED_NAME,
        wszVMDIR_DB_DIR_ATTR_SAM_ACCOUNT_NAME,
        wszVMDIR_DB_DIR_ATTR_SECURITY_DESCRIPTOR,
        wszVMDIR_DB_DIR_ATTR_OBJECT_CLASS,
        wszVMDIR_DB_DIR_ATTR_CREATED_TIME,
        wszVMDIR_DB_DIR_ATTR_OBJECT_SID,
        wszVMDIR_DB_DIR_ATTR_UID,
        wszVMDIR_DB_DIR_ATTR_MEMBERS,
        wszVMDIR_DB_DIR_ATTR_EOL,

#if 0 /* Maybe need to map these attributes. Use above pattern */
        { VMDIR_DB_DIR_ATTR_RECORD_ID },
        { VMDIR_DB_DIR_ATTR_PARENT_DN },
        { VMDIR_DB_DIR_ATTR_DOMAIN },
        { VMDIR_DB_DIR_ATTR_NETBIOS_NAME },
        { VMDIR_DB_DIR_ATTR_USER_PRINCIPAL_NAME },
        { VMDIR_DB_DIR_ATTR_DESCRIPTION },
        { VMDIR_DB_DIR_ATTR_COMMENT },
        { VMDIR_DB_DIR_ATTR_PASSWORD },
        { VMDIR_DB_DIR_ATTR_ACCOUNT_FLAGS },
        { VMDIR_DB_DIR_ATTR_GECOS },
        { VMDIR_DB_DIR_ATTR_HOME_DIR },
        { VMDIR_DB_DIR_ATTR_HOME_DRIVE },
        { VMDIR_DB_DIR_ATTR_LOGON_SCRIPT },
        { VMDIR_DB_DIR_ATTR_PROFILE_PATH },
        { VMDIR_DB_DIR_ATTR_WORKSTATIONS },
        { VMDIR_DB_DIR_ATTR_PARAMETERS },
        { VMDIR_DB_DIR_ATTR_SHELL },
        { VMDIR_DB_DIR_ATTR_PASSWORD_LAST_SET },
        { VMDIR_DB_DIR_ATTR_ALLOW_PASSWORD_CHANGE },
        { VMDIR_DB_DIR_ATTR_FORCE_PASSWORD_CHANGE },
        { VMDIR_DB_DIR_ATTR_FULL_NAME },
        { VMDIR_DB_DIR_ATTR_ACCOUNT_EXPIRY },
        { VMDIR_DB_DIR_ATTR_LM_HASH },
        { VMDIR_DB_DIR_ATTR_NT_HASH },
        { VMDIR_DB_DIR_ATTR_PRIMARY_GROUP },
        { VMDIR_DB_DIR_ATTR_GID },
        { VMDIR_DB_DIR_ATTR_COUNTRY_CODE },
        { VMDIR_DB_DIR_ATTR_CODE_PAGE },
        { VMDIR_DB_DIR_ATTR_MAX_PWD_AGE },
        { VMDIR_DB_DIR_ATTR_MIN_PWD_AGE },
        { VMDIR_DB_DIR_ATTR_PWD_PROMPT_TIME },
        { VMDIR_DB_DIR_ATTR_LAST_LOGON },
        { VMDIR_DB_DIR_ATTR_LAST_LOGOFF },
        { VMDIR_DB_DIR_ATTR_LOCKOUT_TIME },
        { VMDIR_DB_DIR_ATTR_LOGON_COUNT },
        { VMDIR_DB_DIR_ATTR_BAD_PASSWORD_COUNT },
        { VMDIR_DB_DIR_ATTR_LOGON_HOURS },
        { VMDIR_DB_DIR_ATTR_ROLE },
        { VMDIR_DB_DIR_ATTR_MIN_PWD_LENGTH },
        { VMDIR_DB_DIR_ATTR_PWD_HISTORY_LENGTH },
        { VMDIR_DB_DIR_ATTR_PWD_PROPERTIES },
        { VMDIR_DB_DIR_ATTR_FORCE_LOGOFF_TIME },
        { VMDIR_DB_DIR_ATTR_PRIMARY_DOMAIN },
        { VMDIR_DB_DIR_ATTR_SEQUENCE_NUMBER },
        { VMDIR_DB_DIR_ATTR_LOCKOUT_DURATION },
        { VMDIR_DB_DIR_ATTR_LOCKOUT_WINDOW },
        { VMDIR_DB_DIR_ATTR_LOCKOUT_THRESHOLD },
#endif
    };


    for (i=0; wszAttributes[i]; i++)
        ;

    dwMaxEntries = i;

    dwError = LwAllocateMemory(
                  sizeof(VMDIRDB_LDAPATTR_MAP) +
                      sizeof(VMDIRDB_LDAPATTR_MAP_ENTRY) * dwMaxEntries,
                               (VOID *) &pAttrMap);
    BAIL_ON_VMDIRDB_ERROR(dwError);
    
    for (i=0; wszAttributes[i]; i++)
    {
        /* Wide character "SQL" attribute name */
        dwError = LwRtlWC16StringDuplicate(
                      &pAttrMap->attrMap[i].pwszAttribute,
                      wszAttributes[i]);

        /* C String "LDAP" attribute name */
        dwError = LwAllocateString(szAttributes[i],
                                   (VOID *) &pAttrMap->attrMap[i].pszAttribute);
        BAIL_ON_VMDIRDB_ERROR(dwError);

    }
    pAttrMap->dwNumEntries = i;
    pAttrMap->dwMaxEntries = i;
    *ppAttrMap = pAttrMap;

cleanup:
    return dwError;

error:
    VmdirFreeLdapAttributeMap(&pAttrMap);
    goto cleanup;
}

DWORD
VmdirFreeLdapAttributeMap(
    PVMDIRDB_LDAPATTR_MAP *ppAttrMap)
{
    DWORD dwError = 0;
    DWORD i = 0;
    PVMDIRDB_LDAPATTR_MAP pAttrMap = NULL;

    if (!ppAttrMap)
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIRDB_ERROR(dwError);
    }

    pAttrMap = *ppAttrMap;
    if (pAttrMap)
    {
        for (i=0; pAttrMap->dwNumEntries; i++)
        {
            LW_SAFE_FREE_MEMORY(pAttrMap->attrMap[i].pwszAttribute);
            LW_SAFE_FREE_MEMORY(pAttrMap->attrMap[i].pszAttribute);
        }
        LW_SAFE_FREE_MEMORY(pAttrMap);
        *ppAttrMap = NULL;
    }
    
cleanup:
    return dwError;

error:
    goto cleanup;
}


/*
 * Map PWSTR array of "SQL" attributes to PSTR array of "LDAP" 
 * attribute equivalents.
 */
DWORD
VmdirFindLdapAttributeList(
    PWSTR *ppwszAttributes,
    PSTR **pppszLdapAttributes)
{
    DWORD dwError = 0;
    PSTR *ppszLdapAttributes = NULL;
    DWORD i = 0;
    DWORD j = 0;
    DWORD iFound = 0;
    PVMDIRDB_LDAPATTR_MAP pAttrMap = NULL;

    if (!ppwszAttributes || !pppszLdapAttributes || !gVmdirGlobals.pLdapAttrMap)
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIRDB_ERROR(dwError);
    }
    pAttrMap = gVmdirGlobals.pLdapAttrMap;

    for (i=0; ppwszAttributes[i]; i++)
        ;

    dwError = LwAllocateMemory((i+1) * sizeof(PSTR),
                               (VOID *) &ppszLdapAttributes);
    BAIL_ON_VMDIRDB_ERROR(dwError);
    
    for (i=0; ppwszAttributes[i]; i++)
    {
        for (j=0; j < pAttrMap->dwNumEntries; j++)
        {
            if (LwRtlWC16StringIsEqual(ppwszAttributes[i],
                                       (pAttrMap->attrMap[j].pwszAttribute),
                                        FALSE))
            {
                dwError = LwAllocateString(
                              pAttrMap->attrMap[j].pszAttribute,
                              (VOID *) &ppszLdapAttributes[iFound]);
                BAIL_ON_VMDIRDB_ERROR(dwError);
                iFound++;
            }
        }
    }
    *pppszLdapAttributes = ppszLdapAttributes;
    return dwError;

cleanup:
    return dwError;

error:
    VmdirFreeLdapAttributeList(&ppszLdapAttributes);
    goto cleanup;
}


DWORD
VmdirFreeLdapAttributeList(
    PSTR **pppszLdapAttributes)
{
    DWORD dwError = 0;
    DWORD i = 0;
    PSTR *ppszLdapAttributes = NULL;

    if (!pppszLdapAttributes)
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIRDB_ERROR(dwError);
    }

    ppszLdapAttributes = *pppszLdapAttributes;

    for (i=0; ppszLdapAttributes[i]; i++)
    {
        LW_SAFE_FREE_MEMORY(ppszLdapAttributes[i]);
    }
    LW_SAFE_FREE_MEMORY(ppszLdapAttributes);
    *ppszLdapAttributes = NULL;
    

cleanup:
    return dwError;

error:
    goto cleanup;
}
