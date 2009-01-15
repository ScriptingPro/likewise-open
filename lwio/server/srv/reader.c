#include "includes.h"

typedef struct _SMB_SOCKET_READER_WORK_SET
{
    fd_set          fdset;
    int             maxFd;
    PSMBDLINKEDLIST pConnectionList;
} SMB_SOCKET_READER_WORK_SET, *PSMB_SOCKET_READER_WORK_SET;

static
PVOID
SrvSocketReaderMain(
    PVOID pData
    );

static
NTSTATUS
SrvSocketReaderFillFdSet(
    PSMB_SRV_SOCKET_READER_CONTEXT pReaderContext,
    PSMB_SOCKET_READER_FD_CONTEXT pReaderFdContext
    );

static
NTSTATUS
SrvSocketReaderFillFdSetInOrder(
    PVOID pConnection,
    PVOID pUserData,
    PBOOLEAN pbContinue
    );

static
NTSTATUS
SrvSocketReaderProcessConnections(
    PSMB_SRV_SOCKET_READER_CONTEXT pReaderContext,
    PSMB_SOCKET_READER_WORK_SET pReaderWorkset
    );

static
NTSTATUS
SrvSocketReaderReadMessage(
    PSMB_SRV_SOCKET_READER_CONTEXT pReaderContext,
    PSMB_SRV_CONNECTION pConnection
    );

static
VOID
SrvSocketReaderPurgeConnections(
    PSMB_SRV_SOCKET_READER_CONTEXT pReaderContext,
    PSMB_SOCKET_READER_WORK_SET pReaderWorkset
    );

static
NTSTATUS
SrvSocketReaderInterrupt(
    PSMB_SRV_SOCKET_READER pReader
    );

static
BOOLEAN
SrvSocketReaderMustStop(
    PSMB_SRV_SOCKET_READER_CONTEXT pContext
    );

static
int
SrvSocketReaderCompareConnections(
    PVOID pConn1,
    PVOID pConn2
    );

static
VOID
SrvSocketReaderReleaseConnection(
    PVOID pConnection
    );

NTSTATUS
SrvSocketReaderInit(
    PSMB_SRV_SOCKET_READER pReader
    )
{
    NTSTATUS ntStatus = 0;

    memset(&pReader->context, 0, sizeof(pReader->context));

    pReader->context.mutex = PTHREAD_MUTEX_INITIALIZER;
    pReader->context.bStop = FALSE;
    pReader->context.pSocketList  = NULL;
    pReader->context.ulNumSockets = 0;
    pReader->context.fd[0] = pReader->context.fd[1] = -1;

    ntStatus = SMBRBTreeCreate(
                    &SrvSocketReaderCompareConnections,
                    &SrvSocketReaderReleaseConnection,
                    &pReader->context.pConnections);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = SrvProdConsInit(&pReader->context.pWorkQueue);
    BAIL_ON_NT_STATUS(ntStatus);

    if (pipe(pReader->context.fd) < 0)
    {
        // TODO: Map error number
        ntStatus = errno;
    }

    ntStatus = pthread_create(
                    pReader->reader,
                    NULL,
                    &SrvSocketReaderMain,
                    &pReader->context);
    BAIL_ON_NT_STATUS;

    pReader->pReader = &pReader->reader;

error:

    return ntStatus;
}

ULONG
SrvSocketReaderGetCount(
    PSMB_SRV_SOCKET_READER pReader
    )
{
    ULONG ulSockets = 0;

    pthread_mutex_lock(&pReader->pContext.mutex);

    ulSockets = pReader->context.ulNumSockets;

    pthread_mutex_unlock(&pReader->context.mutex);

    return ulSockets;
}

NTSTATUS
SrvSocketReaderEnqueueConnection(
    PSMB_SRV_SOCKET_READER pReader,
    PSMB_SRV_CONNECTION    pConnection
    )
{
    NTSTATUS ntStatus = 0;

    pthread_mutex_lock(&pReader->context.mutex);

    ntStatus = SMBDLinkedListAppend(
                    &pReader->context.pSocketList,
                    pConnection);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = SrvSocketReaderInterrupt(
                    pReader);
    BAIL_ON_NT_STATUS(ntStatus);

cleanup:

    pthread_mutex_unlock(&pReader->context.mutex);

    return ntStatus;

error:

    goto cleanup;
}

static
PVOID
SrvSocketReaderMain(
    PVOID pData
    )
{
    NTSTATUS ntStatus = 0;
    PSMB_SRV_SOCKET_READER_CONTEXT pContext = (PSMB_SRV_SOCKET_READER_CONTEXT)pData;
    SMB_SOCKET_READER_WORK_SET workSet;

    memset(&workSet, 0, sizeof(workSet));

    while (!SrvSocketReaderMustStop(pContext))
    {
        int ret = 0;
        PSMBDLINKEDLIST pIter = NULL;

        ntStatus = SrvSocketReaderFillFdSet(
                        pContext,
                        &workSet);
        BAIL_ON_NT_STATUS(ntStatus);

        ret = select(
                fdContext.maxFd + 1,
                &workSet.fdset,
                NULL,
                &workSet.fdset,
                NULL);
        if (ret == -1)
        {
            // TODO - map error number
            ntStatus = errno;
        }
        else if (ret != 1)
        {
            // TODO - map error number
            ntStatus = EFAULT;
        }
        BAIL_ON_NT_STATUS(ntStatus);

        if (SrvSocketReaderMustStop(pContext))
        {
            break;
        }

        ntStatus = SrvSocketReaderProcessConnections(
                        pContext,
                        &workSet);
        BAIL_ON_NT_STATUS(ntStatus);

        SrvSocketReaderPurgeConnections(
                pContext,
                &workSet);
    }

cleanup:

    if (workSet.pConnectionList)
    {
        SrvSocketReaderPurgeConnections(
                pContext,
                &workSet);
    }

    return NULL;

error:

    goto cleanup;
}

NTSTATUS
SrvSocketReaderFreeContents(
    PSMB_SRV_SOCKET_READER pReader
    )
{
    NTSTATUS ntStatus = 0;
    int iFd = 0;

    if (pReader->pReader)
    {
        pthread_mutex_lock(&pReader->context.mutex);

        pReader->context.bStop = TRUE;

        pthread_mutex_unlock(&pReader->context.mutex);

        ntStatus = SrvSocketReaderInterrupt(pReader);
        BAIL_ON_NT_STATUS(ntStatus);

        pthread_join(pReader->reader);

        pReader->pReader = NULL;
    }

    if (pReader->context.pConnections)
    {
        SMBRBTreeFree(pReader->context.pConnections);
    }

    if (pReader->context.pWorkQueue)
    {
        SrvProdConsFreeContents(pReader->context.pWorkQueue);
    }

    for (; iFd < 2; iFd++)
    {
        if (pReader->context.fd[iFd] >= 0)
        {
            close(pReader->context.fd[iFd]);
        }
    }

    return ntStatus;
}

static
NTSTATUS
SrvSocketReaderFillFdSet(
    PSMB_SRV_SOCKET_READER_CONTEXT pReaderContext,
    PSMB_SOCKET_READER_WORK_SET pReaderWorkset
    )
{
    NTSTATUS ntStatus = 0;
    BOOLEAN bInLock = FALSE;

    FD_ZERO(pReaderWorkset->fdset);
    pReaderWorkset.maxFd = -1;

    FD_SET(pReaderContext->fd[0], &pReaderWorkset->fdset);

    SMB_LOCK_MUTEX(bInLock, &pContext->mutex);

    ntStatus = SMBRBTreeTraverse(
                    pReaderContext->pConnections,
                    SMB_TREE_TRAVERSAL_TYPE_IN_ORDER,
                    &SrvSocketReaderFillFdSetInOrder,
                    pReaderWorkset);
    BAIL_ON_NT_STATUS(ntStatus);

    if (pReaderContext->fd[0] > pReaderWorkset.maxFd)
    {
        pReaderWorkset.maxFd = pReaderContext->fd[0];
    }

cleanup:

    SMB_UNLOCK_MUTEX(bInLock, &pContext->mutex);

    return ntStatus;

error:

    goto cleanup;
}

static
NTSTATUS
SrvSocketReaderFillFdSetInOrder(
    PVOID pConnection,
    PVOID pUserData,
    PBOOLEAN pbContinue
    )
{
    NTSTATUS ntStatus = 0;
    PSMB_SRV_CONNECTION pSrvConnection = (PSMB_SRV_CONNECTION)pConnection;
    PSMB_SOCKET_READER_WORK_SET pReaderWorkset = (PSMB_SOCKET_READER_WORK_SET)pUserData;
    int fd = -1;

    fd = SrvConnectionGetFd(pSrvConnection);

    FD_SET(fd, &pReaderWorkset->fdset);
    pReaderWorkset->maxFd = fd;

    ntStatus = SMBDLinkedListAppend(&pReaderWorkset->pConnectionList, pSrvConnection);
    BAIL_ON_NT_STATUS(ntStatus);

    InterlockedIncrement(&pSrvConnection->refCount);

    *pbContinue = TRUE;

cleanup:

    return ntStatus;

error:

    *pbContinue = FALSE;

    goto cleanup;
}

static
NTSTATUS
SrvSocketReaderProcessConnections(
    PSMB_SRV_SOCKET_READER_CONTEXT pReaderContext,
    PSMB_SOCKET_READER_WORK_SET pReaderWorkset
    )
{
    NTSTATUS ntStatus = 0;
    PSMBDLINKEDLIST pIter = NULL;

    if (FD_ISSET(pReaderContext->fd[0], &pReaderWorkset->fdset))
    {
        CHAR c;

        if (read(pReaderContext->fd[0], &c, 1) != 1)
        {
            ntStatus = STATUS_LOCAL_DISCONNECT;
            BAIL_ON_NT_STATUS(ntStatus);
        }
    }

    for (pIter = pReaderWorkset->pConnectionList;
         pIter;
         pIter = pIter->pNext)
    {
        PSMB_SRV_CONNECTION pConnection = (PSMB_SRV_CONNECTION)pIter->pItem;

        if (FD_ISSET(SrvConnectionGetFd(pConnection), &pReaderWorkset->fdset))
        {
            ntStatus = SrvSocketReaderReadMessage(
                            pReaderContext,
                            pConnection);
            BAIL_ON_NT_STATUS(ntStatus);
        }
    }

cleanup:

    return ntStatus;

error:

    goto cleanup;
}

static
NTSTATUS
SrvSocketReaderReadMessage(
    PSMB_SRV_SOCKET_READER_CONTEXT pReaderContext,
    PSMB_SRV_CONNECTION pConnection
    )
{
    NTSTATUS ntStatus = 0;

    // TODO: Read the message and enqueue it

cleanup:

    return ntStatus;

error:

    goto cleanup;
}

static
VOID
SrvSocketReaderPurgeConnections(
    PSMB_SRV_SOCKET_READER_CONTEXT pReaderContext,
    PSMB_SOCKET_READER_WORK_SET pReaderWorkset
    )
{
    for (pIter = workSet.pConnectionList; pIter; pIter = pIter->pNext)
    {
        PSMB_SRV_CONNECTION pSrvConnection = (PSMB_SRV_CONNECTION)pIter->pItem;

        if (SrvConnectionIsInvalid(pSrvConnection))
        {
            pthread_mutex_lock(&pContext->mutex);

            SMBRBTreeRemove(
                    pContext->pConnections,
                    pSrvConnection);

            pthread_mutex_unlock(&pContext->mutex);
        }

        SrvConnectionRelease(pSrvConnection);
    }

    if (workSet.pConnectionList)
    {
        SMBDLinkedListFree(workSet.pConnectionList);
        workSet.pConnectionList = NULL;
    }
}

static
NTSTATUS
SrvSocketReaderInterrupt(
    PSMB_SRV_SOCKET_READER pReader
    )
{
    NTSTATUS ntStatus = 0;

    if (write(pReader->context.fd[1], 'I', 1) != 1)
    {
        // TODO: Map error number
        ntStatus = errno;
    }

    return ntStatus;
}

static
BOOLEAN
SrvSocketReaderMustStop(
    PSMB_SRV_SOCKET_READER_CONTEXT pContext
    )
{
    BOOLEAN bStop = FALSE;

    pthread_mutex_lock(&pContext->mutex);

    bStop = pContext->bStop;

    pthread_mutex_unlock(&pContext->mutex);

    return bStop;
}

static
int
SrvSocketReaderCompareConnections(
    PVOID pConn1,
    PVOID pConn2
    )
{
    int fd1 = -1;
    int fd2 = -1;

    fd1 = SrvConnectionGetFd((PSMB_SRV_CONNECTION)pConn1);
    fd2 = SrvConnectionGetFd((PSMB_SRV_CONNECTION)pConn2);

    if (fd1 > fd2)
    {
        return 1;
    }
    else if (fd1 < fd2)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

static
VOID
SrvSocketReaderReleaseConnection(
    PVOID pConnection
    )
{
    SrvConnectionRelease((PSMB_SRV_CONNECTION)pConnection);
}
