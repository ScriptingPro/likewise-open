using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;

namespace Likewise.LMC.FileClient
{
#region File Client error, type, and structure definitions
    public enum WinError
    {
        NO_ERROR = 0,
        ERROR_SUCCESS = 0,
        ERROR_FILE_NOT_FOUND = 2,
        ERROR_PATH_NOT_FOUND = 3,
        ERROR_ACCESS_DENIED = 5,
        ERROR_INVALID_HANDLE = 6,
        ERROR_NOT_ENOUGH_MEMORY = 8,
        ERROR_SHARING_VIOLATION = 32,
        ERROR_BAD_DEV_TYPE = 66,
        ERROR_BAD_NET_NAME = 67,
        ERROR_FILE_EXISTS = 80,
        ERROR_ALREADY_ASSIGNED = 85,
        ERROR_INVALID_PASSWORD = 86,
        ERROR_INVALID_PARAMETER = 87,
        ERROR_BUSY = 170,
        ERROR_MORE_DATA = 234,
        ERROR_NO_MORE_ITEMS = 259,
        ERROR_INVALID_ADDRESS = 487,
        ERROR_NO_TOKEN = 1008,
        ERROR_BAD_DEVICE = 1200,
        ERROR_DEVICE_ALREADY_REMEMBERED = 1202,
        ERROR_NO_NET_OR_BAD_PATH = 1203,
        ERROR_BAD_PROVIDER = 1204,
        ERROR_CANNOT_OPEN_PROFILE = 1205,
        ERROR_BAD_PROFILE = 1206,
        ERROR_EXTENDED_ERROR = 1208,
        ERROR_SESSION_CREDENTIAL_CONFLICT = 1219,
        ERROR_NO_NETWORK = 1222,
        ERROR_CANCELLED = 1223,
        ERROR_DOWNGRADE_DETECTED = 1265,
        ERROR_LOGON_FAILURE = 1326,
        ERROR_LOGON_TYPE_NOT_GRANTED = 1385,
        ERROR_BAD_USERNAME = 2202
    };

    public enum ResourceScope
    {
        RESOURCE_CONNECTED = 1,
        RESOURCE_GLOBALNET,
        RESOURCE_REMEMBERED,
        RESOURCE_RECENT,
        RESOURCE_CONTEXT
    };

    public enum ResourceType
    {
        RESOURCETYPE_ANY,
        RESOURCETYPE_DISK,
        RESOURCETYPE_PRINT,
        RESOURCETYPE_RESERVED
    };

    public enum ResourceUsage
    {
        RESOURCEUSAGE_CONNECTABLE = 0x00000001,
        RESOURCEUSAGE_CONTAINER = 0x00000002,
        RESOURCEUSAGE_NOLOCALDEVICE = 0x00000004,
        RESOURCEUSAGE_SIBLING = 0x00000008,
        RESOURCEUSAGE_ATTACHED = 0x00000010,
        RESOURCEUSAGE_ALL = (RESOURCEUSAGE_CONNECTABLE | RESOURCEUSAGE_CONTAINER | RESOURCEUSAGE_ATTACHED)
    };

    public enum ResourceDisplayType
    {
        RESOURCEDISPLAYTYPE_GENERIC = 0,
        RESOURCEDISPLAYTYPE_DOMAIN = 1,
        RESOURCEDISPLAYTYPE_SERVER = 2,
        RESOURCEDISPLAYTYPE_SHARE = 3,
        RESOURCEDISPLAYTYPE_FILE = 4,
        RESOURCEDISPLAYTYPE_GROUP = 5,
        RESOURCEDISPLAYTYPE_NETWORK = 6,
        RESOURCEDISPLAYTYPE_ROOT = 7,
        RESOURCEDISPLAYTYPE_SHAREADMIN = 8,
        RESOURCEDISPLAYTYPE_DIRECTORY = 9,
        RESOURCEDISPLAYTYPE_TREE = 10,
        RESOURCEDISPLAYTYPE_NDSCONTAINER = 10
    };

    [StructLayout(LayoutKind.Sequential)]
    public class NETRESOURCE
    {
        public ResourceScope dwScope = 0;
        public ResourceType dwType = 0;
        public ResourceDisplayType dwDisplayType = 0;
        public ResourceUsage dwUsage = 0;
        [MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)]
        public string pLocalName;
        [MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)]
        public string pRemoteName;
        [MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)]
        public string pComment;
        [MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)]
        public string pProvider;
    };

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public class FileItem
    {
        public bool IsDirectory = false;
        public DateTime CreationTime = new DateTime();
        public DateTime LastAccessTime = new DateTime();
        public DateTime LastWriteTime = new DateTime();
        public UInt64 FileSize = 0;
        public string FileName = null;
        public string Alternate = null;
    };

    public enum FILE_ATTRIBUTE
    {
        FILE_ATTRIBUTE_ARCHIVE = 0x0020,
        FILE_ATTRIBUTE_COMPRESSED = 0x0800,
        FILE_ATTRIBUTE_DEVICE = 0x0040,
        FILE_ATTRIBUTE_DIRECTORY = 0x0010,
        FILE_ATTRIBUTE_ENCRYPTED = 0x4000,
        FILE_ATTRIBUTE_HIDDEN = 0x0002,
        FILE_ATTRIBUTE_NORMAL = 0x0080,
        FILE_ATTRIBUTE_NOT_CONTENT_INDEXED = 0x2000,
        FILE_ATTRIBUTE_OFFLINE = 0x1000,
        FILE_ATTRIBUTE_READONLY = 0x0001,
        FILE_ATTRIBUTE_REPARSE_POINT = 0x0400,
        FILE_ATTRIBUTE_SPARSE_FILE = 0x0200,
        FILE_ATTRIBUTE_SYSTEM = 0x0004,
        FILE_ATTRIBUTE_TEMPORARY = 0x0100,
        FILE_ATTRIBUTE_VIRTUAL = 0x10000
    };

    [StructLayout(LayoutKind.Sequential)]
    public struct FILETIME
    {
        public uint dwLowDateTime;
        public uint dwHighDateTime;
    };

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct WIN32_FIND_DATA
    {
        public FILE_ATTRIBUTE dwFileAttributes;
        public FILETIME ftCreationTime;
        public FILETIME ftLastAccessTime;
        public FILETIME ftLastWriteTime;
        public UInt32 nFileSizeHigh;
        public UInt32 nFileSizeLow;
        public int dwReserved0;
        public int dwReserved1;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)] // public const int MAX_PATH = 260;
        public string cFileName;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 14)] // public const int MAX_ALTERNATE = 14;
        public string cAlternate;
    };

    public struct SYSTEMTIME
    {
        public Int16 wYear;
        public Int16 wMonth;
        public Int16 wDayOfWeek;
        public Int16 wDay;
        public Int16 wHour;
        public Int16 wMinute;
        public Int16 wSecond;
        public Int16 wMilliseconds;
    };

#endregion

    public class FileClient
    {
        protected static bool useWindowsDlls = false;

        public FileClient(
            )
        {
        }

        public static void SetWindowsPlatform(
            )
        {
            useWindowsDlls = true;
        }

        #region Copy/Delete/Move File APIs

        public static WinError apiCopyFile(
            string lpExistingFileName,
            string lpNewFileName,
            bool bFailIfExists
            )
        {
            bool copied = false;
            WinError error = 0;

            if (useWindowsDlls)
            {
                copied = InteropWindows.CopyFile(lpExistingFileName, lpNewFileName, bFailIfExists);
            }
            else
            {
                copied = InteropLikewise.CopyFile(lpExistingFileName, lpNewFileName, bFailIfExists);
            }

            if (!copied)
            {
                error = (WinError)Marshal.GetLastWin32Error();
            }

            return error;
        }

        public static WinError apiCopyDirectory(
               string lpExistingDirectoryName,
               string lpNewDirectoryName,
               bool bFailIfExists
            )
        {
            bool copied = false;
            WinError error = 0;

            if (useWindowsDlls)
            {
                copied = InteropWindows.CopyFile(lpExistingDirectoryName, lpNewDirectoryName, bFailIfExists);
            }
            else
            {
                copied = InteropLikewise.CopyFile(lpExistingDirectoryName, lpNewDirectoryName, bFailIfExists);
            }

            if (!copied)
            {
                error = (WinError)Marshal.GetLastWin32Error();
            }

            return error;
        }

        public static WinError apiDeleteFile(
            string lpFileName
            )
        {
            bool deleted = false;
            WinError error = 0;

            if (useWindowsDlls)
            {
                deleted = InteropWindows.DeleteFile(lpFileName);
            }
            else
            {
                deleted = InteropLikewise.DeleteFile(lpFileName);
            }

            if (!deleted)
            {
                error = (WinError)Marshal.GetLastWin32Error();
            }

            return error;
        }

        public static WinError apiMoveFile(
            string lpExistingFileName,
            string lpNewFileName
            )
        {
            bool moved = false;
            WinError error = 0;

            if (useWindowsDlls)
            {
                moved = InteropWindows.MoveFile(lpExistingFileName, lpNewFileName);
            }
            else
            {
                moved = InteropLikewise.MoveFile(lpExistingFileName, lpNewFileName);
            }

            if (!moved)
            {
                error = (WinError)Marshal.GetLastWin32Error();
            }

            return error;
        }

        public static WinError apiMoveDirectory(
            string lpExistingDirectoryName,
            string lpNewDirectoryName
            )
        {
            bool moved = false;
            WinError error = 0;

            if (useWindowsDlls)
            {
                moved = InteropWindows.MoveFile(lpExistingDirectoryName, lpNewDirectoryName);
            }
            else
            {
                moved = InteropLikewise.MoveFile(lpExistingDirectoryName, lpNewDirectoryName);
            }

            if (!moved)
            {
                error = (WinError)Marshal.GetLastWin32Error();
            }

            return error;
        }

        public static WinError apiRemoveDirectory(
            string lpDirectoryName
            )
        {
            bool deleted = false;
            WinError error = 0;

            if (useWindowsDlls)
            {
                deleted = InteropWindows.DeleteFile(lpDirectoryName);
            }
            else
            {
                deleted = InteropLikewise.DeleteFile(lpDirectoryName);
            }

            if (!deleted)
            {
                error = (WinError)Marshal.GetLastWin32Error();
            }

            return error;
        }

        #endregion

        #region Local and Connected Share File Enumeration APIs

        public static List<FileItem> EnumFiles(
            string filepath,
            bool showHiddenFiles
            )
        {
            List<FileItem> Files = new List<FileItem>();
            IntPtr INVALID_HANDLE_VALUE = new IntPtr(-1);
            WIN32_FIND_DATA pFindFileData;
            bool success = false;
            string search = filepath + "\\*";

            IntPtr handle = INVALID_HANDLE_VALUE;

            if (useWindowsDlls)
            {
                handle = InteropWindows.FindFirstFileW(search, out pFindFileData);
            }
            else
            {
                handle = InteropLikewise.FindFirstFile(search, out pFindFileData);
            }

            if (handle != INVALID_HANDLE_VALUE)
            {
                success = true;
            }

            while (success)
            {
                FileItem file = new FileItem();

                file.FileName = pFindFileData.cFileName;
                file.Alternate = pFindFileData.cAlternate;

                if (String.Compare(file.FileName, ".") == 0 ||
                    String.Compare(file.FileName, "..") == 0)
                {
                    if (useWindowsDlls)
                    {
                        success = InteropWindows.FindNextFileW(handle, out pFindFileData);
                    }
                    else
                    {
                        success = InteropLikewise.FindNextFile(handle, out pFindFileData);
                    }
                    continue;
                }

                if (!showHiddenFiles &&
                    file.FileName[0] == '.')
                {
                    if (useWindowsDlls)
                    {
                        success = InteropWindows.FindNextFileW(handle, out pFindFileData);
                    }
                    else
                    {
                        success = InteropLikewise.FindNextFile(handle, out pFindFileData);
                    }
                    continue;
                }

                if ((pFindFileData.dwFileAttributes & FILE_ATTRIBUTE.FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE.FILE_ATTRIBUTE_DIRECTORY)
                {
                    file.IsDirectory = true;
                }

                UInt64 size = ((UInt64)pFindFileData.nFileSizeLow + (UInt64)pFindFileData.nFileSizeHigh * 4294967296)/1024;
                UInt64 extra = ((UInt64)pFindFileData.nFileSizeLow + (UInt64)pFindFileData.nFileSizeHigh * 4294967296) % 1024;
                SYSTEMTIME created = new SYSTEMTIME();
                SYSTEMTIME modified = new SYSTEMTIME();
                SYSTEMTIME accessed = new SYSTEMTIME();
                DateTime Created = new DateTime();
                DateTime Modified = new DateTime();
                DateTime Accessed = new DateTime();

                if (size != 0 && extra != 0)
                {
                    size++;
                }

                if (pFindFileData.ftCreationTime.dwHighDateTime != 0 &&
                    pFindFileData.ftCreationTime.dwLowDateTime != 0)
                {
                    if (useWindowsDlls)
                    {
                        InteropWindows.FileTimeToSystemTime(ref pFindFileData.ftCreationTime, ref created);
                    }
                    else
                    {
                        InteropLikewise.FileTimeToSystemTime(ref pFindFileData.ftCreationTime, ref created);
                    }
                    Created = new DateTime(created.wYear, created.wMonth, created.wDay, created.wHour, created.wMinute, created.wSecond).ToLocalTime();
                }

                if (pFindFileData.ftLastWriteTime.dwHighDateTime != 0 &&
                    pFindFileData.ftLastWriteTime.dwLowDateTime != 0)
                {
                    if (useWindowsDlls)
                    {
                        InteropWindows.FileTimeToSystemTime(ref pFindFileData.ftLastWriteTime, ref modified);
                    }
                    else
                    {
                        InteropLikewise.FileTimeToSystemTime(ref pFindFileData.ftLastWriteTime, ref modified);
                    }
                    Modified = new DateTime(modified.wYear, modified.wMonth, modified.wDay, modified.wHour, modified.wMinute, modified.wSecond).ToLocalTime();
                }

                if (pFindFileData.ftLastAccessTime.dwHighDateTime != 0 &&
                    pFindFileData.ftLastAccessTime.dwLowDateTime != 0)
                {
                    if (useWindowsDlls)
                    {
                        InteropWindows.FileTimeToSystemTime(ref pFindFileData.ftLastAccessTime, ref accessed);
                    }
                    else
                    {
                        InteropLikewise.FileTimeToSystemTime(ref pFindFileData.ftLastAccessTime, ref accessed);
                    }
                    Accessed = new DateTime(accessed.wYear, accessed.wMonth, accessed.wDay, accessed.wHour, accessed.wMinute, accessed.wSecond).ToLocalTime();
                }

                file.CreationTime = Created;
                file.LastWriteTime = Modified;
                file.LastAccessTime = Accessed;
                file.FileSize = size;

                Files.Add(file);

                if (useWindowsDlls)
                {
                    success = InteropWindows.FindNextFileW(handle, out pFindFileData);
                }
                else
                {
                    success = InteropLikewise.FindNextFile(handle, out pFindFileData);
                }
            }

            return Files;
        }

        #endregion

        #region Remote File Resource Enumeration APIs

        public static WinError CreateConnection(
            string networkName,
            string username,
            string password
            )
        {
            WinError error = WinError.ERROR_SUCCESS;
            NETRESOURCE netResource = new NETRESOURCE();

            netResource.dwScope = ResourceScope.RESOURCE_GLOBALNET;
            netResource.dwType = ResourceType.RESOURCETYPE_DISK;
            netResource.dwDisplayType = ResourceDisplayType.RESOURCEDISPLAYTYPE_SHARE;
            netResource.dwUsage = ResourceUsage.RESOURCEUSAGE_ALL;
            netResource.pRemoteName = networkName;

            if (useWindowsDlls)
            {
                error = InteropWindows.WNetAddConnection2(netResource, password, username, 0);
            }
            else
            {
                error = InteropLikewise.WNetAddConnection2(netResource, password, username, 0);
            }

            return error;
        }

        public static WinError DeleteConnection(
            string networkName
            )
        {
            WinError error = WinError.ERROR_SUCCESS;

            if (useWindowsDlls)
            {
                error = InteropWindows.WNetCancelConnection2(networkName, 0, true);
            }
            else
            {
                error = InteropLikewise.WNetCancelConnection2(networkName, 0, true);
            }

            return error;
        }

        public static WinError BeginEnumNetResources(
            ResourceScope dwScope,
            ResourceType dwType,
            ResourceUsage dwUsage,
            NETRESOURCE pNetResource,
            out IntPtr enumHandle
            )
        {
            WinError error = WinError.ERROR_SUCCESS;

            if (useWindowsDlls)
            {
                error = InteropWindows.WNetOpenEnum(dwScope, dwType, dwUsage, pNetResource, out enumHandle);
            }
            else
            {
                error = WinError.ERROR_INVALID_PARAMETER;
                enumHandle = new IntPtr(0);
            }

            if (error == WinError.ERROR_EXTENDED_ERROR)
            {
                error = (WinError) Marshal.GetLastWin32Error();
            }

            return error;
        }

        public static WinError EnumNetResources(
            IntPtr enumHandle,
            out List<NETRESOURCE> NetResources
            )
        {
            WinError error = WinError.ERROR_SUCCESS;
            List<NETRESOURCE> nrList = new List<NETRESOURCE>();
            int cEntries = -1;
            int bufferSize = 16384;
            IntPtr ptrBuffer = Marshal.AllocHGlobal(bufferSize);
            NETRESOURCE nr;

            for ( ; ; )
            {
                cEntries = -1;
                bufferSize = 16384;

                if (useWindowsDlls)
                {
                    error = InteropWindows.WNetEnumResource(enumHandle, ref cEntries, ptrBuffer, ref bufferSize);
                }
                else
                {
                    error = WinError.ERROR_INVALID_PARAMETER;
                }

                if ((error != WinError.NO_ERROR) || (cEntries < 1))
                {
                    if (error == WinError.ERROR_NO_MORE_ITEMS)
                    {
                        error = WinError.NO_ERROR;
                    }
                    break;
                }

                Int32 ptr = ptrBuffer.ToInt32();
                for (int i = 0; i < cEntries; i++)
                {
                    nr = (NETRESOURCE)Marshal.PtrToStructure(new IntPtr(ptr), typeof(NETRESOURCE));

                    nrList.Add(nr);
                    ptr += Marshal.SizeOf(nr);
                }
            }

            Marshal.FreeHGlobal(ptrBuffer);

            NetResources = nrList;
            return error;
        }

        public static WinError EndEnumNetResources(
            IntPtr enumHandle
            )
        {
            WinError error = WinError.ERROR_SUCCESS;

            if (useWindowsDlls)
            {
                error = InteropWindows.WNetCloseEnum(enumHandle);
            }
            else
            {
                error = WinError.ERROR_INVALID_PARAMETER;
            }

            if (error == WinError.ERROR_EXTENDED_ERROR)
            {
                error = (WinError)Marshal.GetLastWin32Error();
            }

            return error;
        }

        #endregion
    }
}
