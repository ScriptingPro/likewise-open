/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 * -*- mode: c, c-basic-offset: 4 -*- */

/*
 * Copyright Likewise Software    2004-2008
 * All rights reserved.
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

using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using Likewise.LMC.Utilities;
using Likewise.LMC.LDAP.Interop;
using Likewise.LMC.ServerControl;
using Likewise.LMC.LDAP;
using System.Globalization;

namespace Likewise.LMC.Plugins.ADUCPlugin
{
public partial class DomainGeneralEditPage : MPPage, IDirectoryPropertiesPage
{
    #region Class data
    private string Description = string.Empty;
    private ADUCDirectoryNode dirnode = null;
    #endregion
    
    #region Constructors
    public DomainGeneralEditPage()
    {
        this.pageID = "DomainGeneralEditProperities";
        InitializeComponent();
        SetPageTitle("General");
    }
    #endregion
    
    #region Initialization methods
    
    /// <summary>
    /// Queries and fills the ldap message for the Domain
    /// Gets the attribute list from AD for Domain schema attribute.
    /// search for the attributes description
    /// </summary>
    /// <param name="ce"></param>
    /// <param name="servername"></param>
    /// <param name="name"></param>
    /// <param name="dirnode"></param>
    public void SetData(CredentialEntry ce, string servername, string name, ADUCDirectoryNode dirnode)
    {
        try
        {
            this.dirnode = dirnode;
            int ret = -1;
            List<LdapEntry> ldapEntries = null;
            
            ret = dirnode.LdapContext.ListChildEntriesSynchronous
            (dirnode.DistinguishedName,
            LdapAPI.LDAPSCOPE.BASE,
            "(objectClass=*)",
            null,
            false,
            out ldapEntries);
            
            if (ldapEntries == null || ldapEntries.Count == 0)
            {
                return;
            }
            
            LdapEntry ldapNextEntry = ldapEntries[0];
            
            string[] attrsList = ldapNextEntry.GetAttributeNames();
            
            if (attrsList != null)
            {
                foreach (string attr in attrsList)
                {
                    string sValue = "";
                    
                    LdapValue[] attrValues = ldapNextEntry.GetAttributeValues(attr, dirnode.LdapContext);
                    
                    if (attrValues != null && attrValues.Length > 0)
                    {
                        foreach (LdapValue value in attrValues)
                        {
                            sValue = sValue + "," + value.stringData;
                        }
                    }
                    
                    if (sValue.StartsWith(","))
                    {
                        sValue = sValue.Substring(1);
                    }
                    
                    if (string.Compare(sValue, "") == 0)
                    {
                        sValue = "<Not Set>";
                    }
                    
                    if (string.Compare(attr, "description") == 0)
                    {
                        this.txtDescription.Text = sValue;
                        Description = sValue;
                    }
                    
                    //As of now we are not getting canonicalName attribute in the list because of paging issue
                    if (string.Compare(attr, "canonicalName") == 0)
                    {
                        this.Namelabel.Text = sValue.Substring(0, sValue.Length - 1);
                    }
                    
                    if (string.Compare(attr, "name") == 0)
                    {
                        this.textBoxDomainName.Text = sValue.ToUpper();
                        this.Namelabel.Text = sValue;
                    }
                }
                
                this.lblForestLevel.Text = this.labelDomainLevel.Text = "Windows Server 2003";
                
                this.ParentContainer.DataChanged = false;
                this.ParentContainer.btnApply.Enabled = false;
            }
        }
        catch (Exception e)
        {
            container.ShowError(e.Message);
        }
        // throw new NotImplementedException();
    }
    
    /// <summary>
    /// Modifies the specified attributes for the selected Domain
    /// </summary>
    /// <returns></returns>
    public bool OnApply()
    {
        Description = this.txtDescription.Text.Trim();
        List<LDAPMod> attrlist = new List<LDAPMod>();
        //the following portion of code uses openldap "ldap_Modify_s"
        string basedn = dirnode.DistinguishedName;
        DirectoryContext dirContext = dirnode.LdapContext;
        string[] objectClass_values = null;

        if (!String.IsNullOrEmpty(Description))
        {
            objectClass_values = new string[] { Description, null };
        }
        else
        {
            objectClass_values = new string[] { null };
        }
        
        LDAPMod attr =
        new LDAPMod((int)LDAPMod.mod_ops.LDAP_MOD_REPLACE, "description",
        objectClass_values);
        
        LDAPMod[] attrArry = new LDAPMod[] { attr };
        int ret = -1;
        if (attrArry != null && attrArry.Length != 0)
        {
            ret = dirContext.ModifySynchronous(basedn, attrArry);
        }
        else
        {
            return true;
        }
        if (ret != 0)
        {           
            string sMsg = ErrorCodes.LDAPString(ret);
            container.ShowError(sMsg);
            return false;
        }
        this.ParentContainer.DataChanged = false;
        this.ParentContainer.btnApply.Enabled = false;
        return true;
    }
    
    #endregion
    
    #region Events
    
    private void txtDescription_TextChanged(object sender, EventArgs e)
    {
        if (!this.txtDescription.Text.Trim().Equals(Description))
        {
            this.ParentContainer.DataChanged = true;
            this.ParentContainer.btnApply.Enabled = true;
        }
        else
        {
            this.ParentContainer.DataChanged = false;
            this.ParentContainer.btnApply.Enabled = false;
        }
    }
    
    #endregion
}
}
