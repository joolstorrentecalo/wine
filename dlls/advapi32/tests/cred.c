/*
 * Credential Function Tests
 *
 * Copyright 2007 Robert Shearman
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>
#include <stdio.h>

#include "windef.h"
#include "winbase.h"
#include "wincred.h"

#include "wine/test.h"

static BOOL (WINAPI *pCredDeleteA)(LPCSTR,DWORD,DWORD);
static BOOL (WINAPI *pCredEnumerateA)(LPCSTR,DWORD,DWORD *,PCREDENTIALA **);
static VOID (WINAPI *pCredFree)(PVOID);
static BOOL (WINAPI *pCredGetSessionTypes)(DWORD,LPDWORD);
static BOOL (WINAPI *pCredReadA)(LPCSTR,DWORD,DWORD,PCREDENTIALA *);
static BOOL (WINAPI *pCredRenameA)(LPCSTR,LPCSTR,DWORD,DWORD);
static BOOL (WINAPI *pCredWriteA)(PCREDENTIALA,DWORD);
static BOOL (WINAPI *pCredReadDomainCredentialsA)(PCREDENTIAL_TARGET_INFORMATIONA,DWORD,DWORD*,PCREDENTIALA**);
static BOOL (WINAPI *pCredMarshalCredentialA)(CRED_MARSHAL_TYPE,PVOID,LPSTR *);
static BOOL (WINAPI *pCredUnmarshalCredentialA)(LPCSTR,PCRED_MARSHAL_TYPE,PVOID);

#define TEST_TARGET_NAME  "credtest.winehq.org"
#define TEST_TARGET_NAME2 "credtest2.winehq.org"
static const WCHAR TEST_PASSWORD[] = {'p','4','$','$','w','0','r','d','!',0};

static void test_CredReadA(void)
{
    BOOL ret;
    PCREDENTIALA cred;

    SetLastError(0xdeadbeef);
    ret = pCredReadA(TEST_TARGET_NAME, -1, 0, &cred);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
        "CredReadA should have failed with ERROR_INVALID_PARAMETER instead of %d\n",
        GetLastError());

    SetLastError(0xdeadbeef);
    ret = pCredReadA(TEST_TARGET_NAME, CRED_TYPE_GENERIC, 0xdeadbeef, &cred);
    ok(!ret && ( GetLastError() == ERROR_INVALID_FLAGS || GetLastError() == ERROR_INVALID_PARAMETER ),
        "CredReadA should have failed with ERROR_INVALID_FLAGS or ERROR_INVALID_PARAMETER instead of %d\n",
        GetLastError());

    SetLastError(0xdeadbeef);
    ret = pCredReadA(NULL, CRED_TYPE_GENERIC, 0, &cred);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
        "CredReadA should have failed with ERROR_INVALID_PARAMETER instead of %d\n",
        GetLastError());
}

static void test_CredWriteA(void)
{
    CREDENTIALA new_cred;
    BOOL ret;

    SetLastError(0xdeadbeef);
    ret = pCredWriteA(NULL, 0);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
        "CredWriteA should have failed with ERROR_INVALID_PARAMETER instead of %d\n",
        GetLastError());

    new_cred.Flags = 0;
    new_cred.Type = CRED_TYPE_GENERIC;
    new_cred.TargetName = NULL;
    new_cred.Comment = (char *)"Comment";
    new_cred.CredentialBlobSize = 0;
    new_cred.CredentialBlob = NULL;
    new_cred.Persist = CRED_PERSIST_ENTERPRISE;
    new_cred.AttributeCount = 0;
    new_cred.Attributes = NULL;
    new_cred.TargetAlias = NULL;
    new_cred.UserName = (char *)"winetest";

    SetLastError(0xdeadbeef);
    ret = pCredWriteA(&new_cred, 0);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
        "CredWriteA should have failed with ERROR_INVALID_PARAMETER instead of %d\n",
        GetLastError());

    new_cred.TargetName = (char *)TEST_TARGET_NAME;
    new_cred.Type = CRED_TYPE_DOMAIN_PASSWORD;

    SetLastError(0xdeadbeef);
    ret = pCredWriteA(&new_cred, 0);
    if (ret)
    {
        ok(GetLastError() == ERROR_SUCCESS ||
           GetLastError() == ERROR_IO_PENDING, /* Vista */
           "Expected ERROR_IO_PENDING, got %d\n", GetLastError());
    }
    else
    {
        ok(GetLastError() == ERROR_BAD_USERNAME ||
           GetLastError() == ERROR_NO_SUCH_LOGON_SESSION, /* Vista */
           "CredWrite with username without domain should return ERROR_BAD_USERNAME"
           "or ERROR_NO_SUCH_LOGON_SESSION not %d\n", GetLastError());
    }

    new_cred.UserName = NULL;
    SetLastError(0xdeadbeef);
    ret = pCredWriteA(&new_cred, 0);
    ok(!ret && GetLastError() == ERROR_BAD_USERNAME,
        "CredWriteA with NULL username should have failed with ERROR_BAD_USERNAME instead of %d\n",
        GetLastError());
}

static void test_CredDeleteA(void)
{
    BOOL ret;

    SetLastError(0xdeadbeef);
    ret = pCredDeleteA(TEST_TARGET_NAME, -1, 0);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
        "CredDeleteA should have failed with ERROR_INVALID_PARAMETER instead of %d\n",
        GetLastError());

    SetLastError(0xdeadbeef);
    ret = pCredDeleteA(TEST_TARGET_NAME, CRED_TYPE_GENERIC, 0xdeadbeef);
    ok(!ret && ( GetLastError() == ERROR_INVALID_FLAGS || GetLastError() == ERROR_INVALID_PARAMETER /* Vista */ ),
        "CredDeleteA should have failed with ERROR_INVALID_FLAGS or ERROR_INVALID_PARAMETER instead of %d\n",
        GetLastError());
}

static void test_CredReadDomainCredentialsA(void)
{
    BOOL ret;
    char target_name[] = "no_such_target";
    CREDENTIAL_TARGET_INFORMATIONA info = {target_name, NULL, target_name, NULL, NULL, NULL, NULL, 0, 0, NULL};
    DWORD count;
    PCREDENTIAL* creds;

    if (!pCredReadDomainCredentialsA)
    {
        win_skip("CredReadDomainCredentialsA() is not implemented\n");
        return;
    }

    /* these two tests would crash on both native and Wine. Implementations
     * does not check for NULL output pointers and try to zero them out early */
if(0)
{
    ret = pCredReadDomainCredentialsA(&info, 0, NULL, &creds);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "!\n");
    ret = pCredReadDomainCredentialsA(&info, 0, &count, NULL);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "!\n");
}

    SetLastError(0xdeadbeef);
    ret = pCredReadDomainCredentialsA(NULL, 0, &count, &creds);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
        "CredReadDomainCredentialsA should have failed with ERROR_INVALID_PARAMETER instead of %d\n",
        GetLastError());

    SetLastError(0xdeadbeef);
    creds = (void*)0x12345;
    count = 2;
    ret = pCredReadDomainCredentialsA(&info, 0, &count, &creds);
    ok(!ret && GetLastError() == ERROR_NOT_FOUND,
        "CredReadDomainCredentialsA should have failed with ERROR_NOT_FOUND instead of %d\n",
        GetLastError());
    ok(count ==0 && creds == NULL, "CredReadDomainCredentialsA must not return any result\n");

    info.TargetName = NULL;

    SetLastError(0xdeadbeef);
    ret = pCredReadDomainCredentialsA(&info, 0, &count, &creds);
    ok(!ret, "CredReadDomainCredentialsA should have failed\n");
    ok(GetLastError() == ERROR_NOT_FOUND ||
        GetLastError() == ERROR_INVALID_PARAMETER, /* Vista, W2K8 */
        "Expected ERROR_NOT_FOUND or ERROR_INVALID_PARAMETER instead of %d\n",
        GetLastError());

    info.DnsServerName = NULL;

    SetLastError(0xdeadbeef);
    ret = pCredReadDomainCredentialsA(&info, 0, &count, &creds);
    ok(!ret, "CredReadDomainCredentialsA should have failed\n");
    ok(GetLastError() == ERROR_NOT_FOUND ||
        GetLastError() == ERROR_INVALID_PARAMETER, /* Vista, W2K8 */
        "Expected ERROR_NOT_FOUND or ERROR_INVALID_PARAMETER instead of %d\n",
        GetLastError());
}

static void check_blob(int line, DWORD cred_type, PCREDENTIALA cred)
{
    if (cred_type == CRED_TYPE_DOMAIN_PASSWORD)
    {
        todo_wine
        ok_(__FILE__, line)(cred->CredentialBlobSize == 0, "expected CredentialBlobSize of 0 but got %d\n", cred->CredentialBlobSize);
        todo_wine
        ok_(__FILE__, line)(!cred->CredentialBlob, "expected NULL credentials but got %p\n", cred->CredentialBlob);
    }
    else
    {
        DWORD size=sizeof(TEST_PASSWORD);
        ok_(__FILE__, line)(cred->CredentialBlobSize == size, "expected CredentialBlobSize of %u but got %u\n", size, cred->CredentialBlobSize);
        ok_(__FILE__, line)(cred->CredentialBlob != NULL, "CredentialBlob should be present\n");
        if (cred->CredentialBlob)
            ok_(__FILE__, line)(!memcmp(cred->CredentialBlob, TEST_PASSWORD, size), "wrong CredentialBlob\n");
    }
}

static void test_generic(void)
{
    BOOL ret;
    DWORD count, i;
    PCREDENTIALA *creds;
    CREDENTIALA new_cred;
    PCREDENTIALA cred;
    BOOL found = FALSE;

    new_cred.Flags = 0;
    new_cred.Type = CRED_TYPE_GENERIC;
    new_cred.TargetName = (char *)TEST_TARGET_NAME;
    new_cred.Comment = (char *)"Comment";
    new_cred.CredentialBlobSize = sizeof(TEST_PASSWORD);
    new_cred.CredentialBlob = (LPBYTE)TEST_PASSWORD;
    new_cred.Persist = CRED_PERSIST_ENTERPRISE;
    new_cred.AttributeCount = 0;
    new_cred.Attributes = NULL;
    new_cred.TargetAlias = NULL;
    new_cred.UserName = (char *)"winetest";

    ret = pCredWriteA(&new_cred, 0);
    ok(ret || broken(GetLastError() == ERROR_NO_SUCH_LOGON_SESSION),
       "CredWriteA failed with error %d\n", GetLastError());
    if (!ret)
    {
        skip("couldn't write generic credentials, skipping tests\n");
        return;
    }

    ret = pCredEnumerateA(NULL, 0, &count, &creds);
    ok(ret, "CredEnumerateA failed with error %d\n", GetLastError());

    for (i = 0; i < count; i++)
    {
        if (!strcmp(creds[i]->TargetName, TEST_TARGET_NAME))
        {
            ok(creds[i]->Type == CRED_TYPE_GENERIC ||
               creds[i]->Type == CRED_TYPE_DOMAIN_PASSWORD, /* Vista */
               "expected creds[%d]->Type CRED_TYPE_GENERIC or CRED_TYPE_DOMAIN_PASSWORD but got %d\n", i, creds[i]->Type);
            ok(!creds[i]->Flags, "expected creds[%d]->Flags 0 but got 0x%x\n", i, creds[i]->Flags);
            ok(!strcmp(creds[i]->Comment, "Comment"), "expected creds[%d]->Comment \"Comment\" but got \"%s\"\n", i, creds[i]->Comment);
            check_blob(__LINE__, creds[i]->Type, creds[i]);
            ok(creds[i]->Persist, "expected creds[%d]->Persist CRED_PERSIST_ENTERPRISE but got %d\n", i, creds[i]->Persist);
            ok(!strcmp(creds[i]->UserName, "winetest"), "expected creds[%d]->UserName \"winetest\" but got \"%s\"\n", i, creds[i]->UserName);
            found = TRUE;
        }
    }
    pCredFree(creds);
    ok(found, "credentials not found\n");

    ret = pCredReadA(TEST_TARGET_NAME, CRED_TYPE_GENERIC, 0, &cred);
    ok(ret, "CredReadA failed with error %d\n", GetLastError());
    pCredFree(cred);

    ret = pCredDeleteA(TEST_TARGET_NAME, CRED_TYPE_GENERIC, 0);
    ok(ret, "CredDeleteA failed with error %d\n", GetLastError());
}

static void test_domain_password(DWORD cred_type)
{
    BOOL ret;
    DWORD count, i;
    PCREDENTIALA *creds;
    CREDENTIALA new_cred;
    PCREDENTIALA cred;
    BOOL found = FALSE;

    new_cred.Flags = 0;
    new_cred.Type = cred_type;
    new_cred.TargetName = (char *)TEST_TARGET_NAME;
    new_cred.Comment = (char *)"Comment";
    new_cred.CredentialBlobSize = sizeof(TEST_PASSWORD);
    new_cred.CredentialBlob = (LPBYTE)TEST_PASSWORD;
    new_cred.Persist = CRED_PERSIST_ENTERPRISE;
    new_cred.AttributeCount = 0;
    new_cred.Attributes = NULL;
    new_cred.TargetAlias = NULL;
    new_cred.UserName = (char *)"test\\winetest";
    ret = pCredWriteA(&new_cred, 0);
    if (!ret && GetLastError() == ERROR_NO_SUCH_LOGON_SESSION)
    {
        skip("CRED_TYPE_DOMAIN_PASSWORD credentials are not supported "
             "or are disabled. Skipping\n");
        return;
    }
    ok(ret, "CredWriteA failed with error %d\n", GetLastError());

    ret = pCredEnumerateA(NULL, 0, &count, &creds);
    ok(ret, "CredEnumerateA failed with error %d\n", GetLastError());

    for (i = 0; i < count; i++)
    {
        if (!strcmp(creds[i]->TargetName, TEST_TARGET_NAME))
        {
            ok(creds[i]->Type == cred_type, "expected creds[%d]->Type CRED_TYPE_DOMAIN_PASSWORD but got %d\n", i, creds[i]->Type);
            ok(!creds[i]->Flags, "expected creds[%d]->Flags 0 but got 0x%x\n", i, creds[i]->Flags);
            ok(!strcmp(creds[i]->Comment, "Comment"), "expected creds[%d]->Comment \"Comment\" but got \"%s\"\n", i, creds[i]->Comment);
            check_blob(__LINE__, cred_type, creds[i]);
            ok(creds[i]->Persist, "expected creds[%d]->Persist CRED_PERSIST_ENTERPRISE but got %d\n", i, creds[i]->Persist);
            ok(!strcmp(creds[i]->UserName, "test\\winetest"), "expected creds[%d]->UserName \"winetest\" but got \"%s\"\n", i, creds[i]->UserName);
            found = TRUE;
        }
    }
    pCredFree(creds);
    ok(found, "credentials not found\n");

    ret = pCredReadA(TEST_TARGET_NAME, cred_type, 0, &cred);
    ok(ret, "CredReadA failed with error %d\n", GetLastError());
    if (ret)  /* don't check the values of cred, if CredReadA failed. */
    {
        check_blob(__LINE__, cred_type, cred);
        pCredFree(cred);
    }

    ret = pCredDeleteA(TEST_TARGET_NAME, cred_type, 0);
    ok(ret, "CredDeleteA failed with error %d\n", GetLastError());
}

static void test_CredMarshalCredentialA(void)
{
    static WCHAR emptyW[] = {0};
    static WCHAR tW[] = {'t',0};
    static WCHAR teW[] = {'t','e',0};
    static WCHAR tesW[] = {'t','e','s',0};
    static WCHAR testW[] = {'t','e','s','t',0};
    static WCHAR test1W[] = {'t','e','s','t','1',0};
    CERT_CREDENTIAL_INFO cert;
    USERNAME_TARGET_CREDENTIAL_INFO username;
    DWORD error;
    char *str;
    BOOL ret;

    SetLastError( 0xdeadbeef );
    ret = pCredMarshalCredentialA( 0, NULL, NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    memset( cert.rgbHashOfCert, 0, sizeof(cert.rgbHashOfCert) );
    cert.cbSize = sizeof(cert);
    SetLastError( 0xdeadbeef );
    ret = pCredMarshalCredentialA( 0, &cert, NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    str = (char *)0xdeadbeef;
    SetLastError( 0xdeadbeef );
    ret = pCredMarshalCredentialA( 0, &cert, &str );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );
    ok( str == (char *)0xdeadbeef, "got %p\n", str );

    SetLastError( 0xdeadbeef );
    ret = pCredMarshalCredentialA( CertCredential, NULL, NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    if (0) { /* crash */
    SetLastError( 0xdeadbeef );
    ret = pCredMarshalCredentialA( CertCredential, &cert, NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );
    }

    cert.cbSize = 0;
    str = (char *)0xdeadbeef;
    SetLastError( 0xdeadbeef );
    ret = pCredMarshalCredentialA( CertCredential, &cert, &str );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );
    ok( str == (char *)0xdeadbeef, "got %p\n", str );

    cert.cbSize = sizeof(cert) + 4;
    str = NULL;
    ret = pCredMarshalCredentialA( CertCredential, &cert, &str );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( str != NULL, "str not set\n" );
    ok( !lstrcmpA( str, "@@BAAAAAAAAAAAAAAAAAAAAAAAAAAA" ), "got %s\n", str );
    pCredFree( str );

    cert.cbSize = sizeof(cert);
    cert.rgbHashOfCert[0] = 2;
    str = NULL;
    ret = pCredMarshalCredentialA( CertCredential, &cert, &str );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( str != NULL, "str not set\n" );
    ok( !lstrcmpA( str, "@@BCAAAAAAAAAAAAAAAAAAAAAAAAAA" ), "got %s\n", str );
    pCredFree( str );

    cert.rgbHashOfCert[0] = 255;
    str = NULL;
    ret = pCredMarshalCredentialA( CertCredential, &cert, &str );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( str != NULL, "str not set\n" );
    ok( !lstrcmpA( str, "@@B-DAAAAAAAAAAAAAAAAAAAAAAAAA" ), "got %s\n", str );
    pCredFree( str );

    cert.rgbHashOfCert[0] = 1;
    cert.rgbHashOfCert[1] = 1;
    str = NULL;
    ret = pCredMarshalCredentialA( CertCredential, &cert, &str );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( str != NULL, "str not set\n" );
    ok( !lstrcmpA( str, "@@BBEAAAAAAAAAAAAAAAAAAAAAAAAA" ), "got %s\n", str );
    pCredFree( str );

    cert.rgbHashOfCert[0] = 1;
    cert.rgbHashOfCert[1] = 1;
    cert.rgbHashOfCert[2] = 1;
    str = NULL;
    ret = pCredMarshalCredentialA( CertCredential, &cert, &str );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( str != NULL, "str not set\n" );
    ok( !lstrcmpA( str, "@@BBEQAAAAAAAAAAAAAAAAAAAAAAAA" ), "got %s\n", str );
    pCredFree( str );

    memset( cert.rgbHashOfCert, 0, sizeof(cert.rgbHashOfCert) );
    cert.rgbHashOfCert[0] = 'W';
    cert.rgbHashOfCert[1] = 'i';
    cert.rgbHashOfCert[2] = 'n';
    cert.rgbHashOfCert[3] = 'e';
    str = NULL;
    ret = pCredMarshalCredentialA( CertCredential, &cert, &str );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( str != NULL, "str not set\n" );
    ok( !lstrcmpA( str, "@@BXlmblBAAAAAAAAAAAAAAAAAAAAA" ), "got %s\n", str );
    pCredFree( str );

    memset( cert.rgbHashOfCert, 0xff, sizeof(cert.rgbHashOfCert) );
    str = NULL;
    ret = pCredMarshalCredentialA( CertCredential, &cert, &str );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( str != NULL, "str not set\n" );
    ok( !lstrcmpA( str, "@@B--------------------------P" ), "got %s\n", str );
    pCredFree( str );

    username.UserName = NULL;
    str = (char *)0xdeadbeef;
    SetLastError( 0xdeadbeef );
    ret = pCredMarshalCredentialA( UsernameTargetCredential, &username, &str );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );
    ok( str == (char *)0xdeadbeef, "got %p\n", str );

    username.UserName = emptyW;
    str = (char *)0xdeadbeef;
    SetLastError( 0xdeadbeef );
    ret = pCredMarshalCredentialA( UsernameTargetCredential, &username, &str );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );
    ok( str == (char *)0xdeadbeef, "got %p\n", str );

    username.UserName = tW;
    str = NULL;
    ret = pCredMarshalCredentialA( UsernameTargetCredential, &username, &str );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( str != NULL, "str not set\n" );
    ok( !lstrcmpA( str, "@@CCAAAAA0BA" ), "got %s\n", str );
    pCredFree( str );

    username.UserName = teW;
    str = NULL;
    ret = pCredMarshalCredentialA( UsernameTargetCredential, &username, &str );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( str != NULL, "str not set\n" );
    ok( !lstrcmpA( str, "@@CEAAAAA0BQZAA" ), "got %s\n", str );
    pCredFree( str );

    username.UserName = tesW;
    str = NULL;
    ret = pCredMarshalCredentialA( UsernameTargetCredential, &username, &str );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( str != NULL, "str not set\n" );
    ok( !lstrcmpA( str, "@@CGAAAAA0BQZAMHA" ), "got %s\n", str );
    pCredFree( str );

    username.UserName = testW;
    str = NULL;
    ret = pCredMarshalCredentialA( UsernameTargetCredential, &username, &str );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( str != NULL, "str not set\n" );
    ok( !lstrcmpA( str, "@@CIAAAAA0BQZAMHA0BA" ), "got %s\n", str );
    pCredFree( str );

    username.UserName = test1W;
    str = NULL;
    ret = pCredMarshalCredentialA( UsernameTargetCredential, &username, &str );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( str != NULL, "str not set\n" );
    ok( !lstrcmpA( str, "@@CKAAAAA0BQZAMHA0BQMAA" ), "got %s\n", str );
    pCredFree( str );
}

static void test_CredUnmarshalCredentialA(void)
{
    static WCHAR tW[] = {'t',0};
    static WCHAR testW[] = {'t','e','s','t',0};
    CERT_CREDENTIAL_INFO *cert;
    USERNAME_TARGET_CREDENTIAL_INFO *username;
    CRED_MARSHAL_TYPE type;
    unsigned int i;
    DWORD error;
    BOOL ret;

    SetLastError( 0xdeadbeef );
    ret = pCredUnmarshalCredentialA( NULL, NULL, NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    cert = NULL;
    SetLastError( 0xdeadbeef );
    ret = pCredUnmarshalCredentialA( NULL, NULL, (void **)&cert );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    type = 0;
    cert = NULL;
    SetLastError( 0xdeadbeef );
    ret = pCredUnmarshalCredentialA( NULL, &type, (void **)&cert );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    type = 0;
    cert = NULL;
    SetLastError( 0xdeadbeef );
    ret = pCredUnmarshalCredentialA( "", &type, (void **)&cert );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    if (0) { /* crash */
    SetLastError( 0xdeadbeef );
    ret = pCredUnmarshalCredentialA( "@@BAAAAAAAAAAAAAAAAAAAAAAAAAAA", &type, NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    SetLastError( 0xdeadbeef );
    ret = pCredUnmarshalCredentialA( "@@BAAAAAAAAAAAAAAAAAAAAAAAAAAA", NULL, (void **)&cert );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );
    }

    type = 0;
    cert = NULL;
    ret = pCredUnmarshalCredentialA( "@@BAAAAAAAAAAAAAAAAAAAAAAAAAAA", &type, (void **)&cert );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( type == CertCredential, "got %u\n", type );
    ok( cert != NULL, "cert is NULL\n" );
    ok( cert->cbSize == sizeof(*cert), "wrong size %u\n", cert->cbSize );
    for (i = 0; i < sizeof(cert->rgbHashOfCert); i++) ok( !cert->rgbHashOfCert[i], "wrong data\n" );
    pCredFree( cert );

    type = 0;
    cert = NULL;
    ret = pCredUnmarshalCredentialA( "@@BXlmblBAAAAAAAAAAAAAAAAAAAAA", &type, (void **)&cert );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( type == CertCredential, "got %u\n", type );
    ok( cert != NULL, "cert is NULL\n" );
    ok( cert->cbSize == sizeof(*cert), "wrong size %u\n", cert->cbSize );
    ok( cert->rgbHashOfCert[0] == 'W', "wrong data)\n" );
    ok( cert->rgbHashOfCert[1] == 'i', "wrong data\n" );
    ok( cert->rgbHashOfCert[2] == 'n', "wrong data\n" );
    ok( cert->rgbHashOfCert[3] == 'e', "wrong data\n" );
    for (i = 4; i < sizeof(cert->rgbHashOfCert); i++) ok( !cert->rgbHashOfCert[i], "wrong data\n" );
    pCredFree( cert );

    SetLastError( 0xdeadbeef );
    ret = pCredUnmarshalCredentialA( "@@CAAAAAA", &type, (void **)&username );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    SetLastError( 0xdeadbeef );
    ret = pCredUnmarshalCredentialA( "@@CAAAAAA0BA", &type, (void **)&username );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    type = 0;
    username = NULL;
    ret = pCredUnmarshalCredentialA( "@@CCAAAAA0BA", &type, (void **)&username );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( type == UsernameTargetCredential, "got %u\n", type );
    ok( username != NULL, "username is NULL\n" );
    ok( username->UserName != NULL, "UserName is NULL\n" );
    ok( !lstrcmpW( username->UserName, tW ), "got %s\n", wine_dbgstr_w(username->UserName) );
    pCredFree( username );

    type = 0;
    username = NULL;
    ret = pCredUnmarshalCredentialA( "@@CIAAAAA0BQZAMHA0BA", &type, (void **)&username );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( type == UsernameTargetCredential, "got %u\n", type );
    ok( username != NULL, "username is NULL\n" );
    ok( username->UserName != NULL, "UserName is NULL\n" );
    ok( !lstrcmpW( username->UserName, testW ), "got %s\n", wine_dbgstr_w(username->UserName) );
    pCredFree( username );
}

START_TEST(cred)
{
    DWORD persists[CRED_TYPE_MAXIMUM];

    pCredEnumerateA = (void *)GetProcAddress(GetModuleHandle("advapi32.dll"), "CredEnumerateA");
    pCredFree = (void *)GetProcAddress(GetModuleHandle("advapi32.dll"), "CredFree");
    pCredGetSessionTypes = (void *)GetProcAddress(GetModuleHandle("advapi32.dll"), "CredGetSessionTypes");
    pCredWriteA = (void *)GetProcAddress(GetModuleHandle("advapi32.dll"), "CredWriteA");
    pCredDeleteA = (void *)GetProcAddress(GetModuleHandle("advapi32.dll"), "CredDeleteA");
    pCredReadA = (void *)GetProcAddress(GetModuleHandle("advapi32.dll"), "CredReadA");
    pCredRenameA = (void *)GetProcAddress(GetModuleHandle("advapi32.dll"), "CredRenameA");
    pCredReadDomainCredentialsA = (void *)GetProcAddress(GetModuleHandle("advapi32.dll"), "CredReadDomainCredentialsA");
    pCredMarshalCredentialA = (void *)GetProcAddress(GetModuleHandle("advapi32.dll"), "CredMarshalCredentialA");
    pCredUnmarshalCredentialA = (void *)GetProcAddress(GetModuleHandle("advapi32.dll"), "CredUnmarshalCredentialA");

    if (!pCredEnumerateA || !pCredFree || !pCredWriteA || !pCredDeleteA || !pCredReadA)
    {
        win_skip("credentials functions not present in advapi32.dll\n");
        return;
    }

    if (pCredGetSessionTypes)
    {
        BOOL ret;
        DWORD i;
        ret = pCredGetSessionTypes(CRED_TYPE_MAXIMUM, persists);
        ok(ret, "CredGetSessionTypes failed with error %d\n", GetLastError());
        ok(persists[0] == CRED_PERSIST_NONE, "persists[0] = %u instead of CRED_PERSIST_NONE\n", persists[0]);
        for (i=0; i < CRED_TYPE_MAXIMUM; i++)
            ok(persists[i] <= CRED_PERSIST_ENTERPRISE, "bad value for persists[%u]: %u\n", i, persists[i]);
    }
    else
        memset(persists, CRED_PERSIST_ENTERPRISE, sizeof(persists));

    test_CredReadA();
    test_CredWriteA();
    test_CredDeleteA();

    test_CredReadDomainCredentialsA();

    trace("generic:\n");
    if (persists[CRED_TYPE_GENERIC] == CRED_PERSIST_NONE)
        skip("CRED_TYPE_GENERIC credentials are not supported or are disabled. Skipping\n");
    else
        test_generic();

        trace("domain password:\n");
    if (persists[CRED_TYPE_DOMAIN_PASSWORD] == CRED_PERSIST_NONE)
        skip("CRED_TYPE_DOMAIN_PASSWORD credentials are not supported or are disabled. Skipping\n");
    else
        test_domain_password(CRED_TYPE_DOMAIN_PASSWORD);

    trace("domain visible password:\n");
    if (persists[CRED_TYPE_DOMAIN_VISIBLE_PASSWORD] == CRED_PERSIST_NONE)
        skip("CRED_TYPE_DOMAIN_VISIBLE_PASSWORD credentials are not supported or are disabled. Skipping\n");
    else
        test_domain_password(CRED_TYPE_DOMAIN_VISIBLE_PASSWORD);

    test_CredMarshalCredentialA();
    test_CredUnmarshalCredentialA();
}
