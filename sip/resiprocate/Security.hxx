#if !defined(RESIP_SECURITY_HXX)
#define RESIP_SECURITY_HXX


#include <map>
#include <vector>

#include "resiprocate/os/Socket.hxx"
#include "resiprocate/os/BaseException.hxx"
#include "resiprocate/SecurityTypes.hxx"
#include "resiprocate/SecurityAttributes.hxx"

#if defined(USE_SSL)
#include <openssl/ssl.h>
#else
// to ensure compilation and object size invariance.
typedef void BIO;
typedef void SSL;
typedef void X509;
typedef void X509_STORE;
typedef void SSL_CTX;
typedef void EVP_PKEY;
#endif

namespace resip
{

class Contents;
class Pkcs7Contents;
class Security;
class MultipartSignedContents;
class SipMessage;

class BaseSecurity
{
   public:
      class Exception : public BaseException
      {
         public:
            Exception(const Data& msg, const Data& file, const int line);
            const char* name() const { return "SecurityException"; }
      };

      BaseSecurity();
      virtual ~BaseSecurity();

      // used to initialize the openssl library
      static void initialize();

      typedef enum
      {
         RootCert,
         DomainCert,
         DomainPrivateKey,
         UserCert,
         UserPrivateKey
      } PEMType;

      virtual void preload()=0;

      // name refers to the domainname or username which could be converted to a
      // filename by convention
      virtual void onReadPEM(const Data& name, PEMType type, Data& buffer) const =0;
      virtual void onWritePEM(const Data& name, PEMType type, const Data& buffer) const =0;
      virtual void onRemovePEM(const Data& name, PEMType type) const =0;

      struct CertificateInfo
      {
            Data name;
            Data fingerprint;
            Data validFrom;
            Data validTo;
      };
      std::vector<CertificateInfo> getRootCertDescriptions() const;

      // All of these guys can throw SecurityException

      void addRootCertPEM(const Data& x509PEMEncodedRootCerts);

      void addDomainCertPEM(const Data& domainName, const Data& certPEM);
      void addDomainCertDER(const Data& domainName, const Data& certDER);
      bool hasDomainCert(const Data& domainName) const;
      bool removeDomainCert(const Data& domainName);
      Data getDomainCertDER(const Data& domainName) const;

      void addDomainPrivateKeyPEM(const Data& domainName, const Data& privateKeyPEM);
      bool hasDomainPrivateKey(const Data& domainName) const;
      bool removeDomainPrivateKey(const Data& domainName);
      Data getDomainPrivateKeyPEM(const Data& domainName) const;

      void addUserCertPEM(const Data& aor, const Data& certPEM);
      void addUserCertDER(const Data& aor, const Data& certDER);
      bool hasUserCert(const Data& aor) const;
      bool removeUserCert(const Data& aor);
      Data getUserCertDER(const Data& aor) const;

      void setUserPassPhrase(const Data& aor, const Data& passPhrase);
      bool hasUserPassPhrase(const Data& aor) const;
      bool removeUserPassPhrase(const Data& aor);
      Data getUserPassPhrase(const Data& aor) const;

      void addUserPrivateKeyPEM(const Data& aor, const Data& certPEM);
      void addUserPrivateKeyDER(const Data& aor, const Data& certDER);
      bool hasUserPrivateKey(const Data& aor) const;
      bool removeUserPrivateKey(const Data& aor);
      Data getUserPrivateKeyPEM(const Data& aor) const;
      Data getUserPrivateKeyDER(const Data& aor) const;

      void generateUserCert (const Data& aor, const Data& passPhrase);

      // produces a detached signature
      MultipartSignedContents* sign(const Data& senderAor, Contents* );
      Pkcs7Contents* encrypt(Contents* , const Data& recipCertName );
      Pkcs7Contents* signAndEncrypt( const Data& senderAor, Contents* , const Data& recipCertName );

      Data computeIdentity( const Data& signerDomain, const Data& in ) const;
      bool checkIdentity( const Data& signerDomain, const Data& in, const Data& sig ) const;

      void checkAndSetIdentity( const SipMessage& msg ) const;

      // returns NULL if it fails
      Contents* decrypt( const Data& decryptorAor, Pkcs7Contents* );
      
      // returns NULL if fails. returns the data that was originally signed
      Contents* checkSignature( MultipartSignedContents*, Data* signedBy, SignatureStatus* sigStat );

      // map of name to certificates
      typedef std::map<Data,X509*>     X509Map;
      typedef std::map<Data,EVP_PKEY*> PrivateKeyMap;
      typedef std::map<Data,Data>      PassPhraseMap;

   private:
      SSL_CTX*       mTlsCtx;
      SSL_CTX*       mSslCtx;
      static void dumpAsn(char*, Data);

      // root cert list
      X509_STORE*    mRootCerts;

      mutable X509Map        mDomainCerts;
      mutable PrivateKeyMap  mDomainPrivateKeys;

      mutable X509Map        mUserCerts;
      mutable PassPhraseMap  mUserPassPhrases;
      mutable PrivateKeyMap  mUserPrivateKeys;

      void addCertPEM (PEMType type, const Data& key, const Data& certPEM, bool write);
      void addCertDER (PEMType type, const Data& key, const Data& certDER, bool write);
      bool hasCert (PEMType type, const Data& key, bool read) const;
      bool removeCert (PEMType type, const Data& key, bool remove);
      Data getCertDER (PEMType type, const Data& key, bool read) const;

      void addPrivateKeyPEM (PEMType type, const Data& key, const Data& privateKeyPEM, bool write);
      void addPrivateKeyDER (PEMType type, const Data& key, const Data& privateKeyDER, bool write);
      bool hasPrivateKey (PEMType type, const Data& key, bool read) const;
      bool removePrivateKey (PEMType type, const Data& key, bool remove);
      Data getPrivateKeyPEM (PEMType type, const Data& key, bool read) const;
      Data getPrivateKeyDER (PEMType type, const Data& key, bool read) const;

      //===========================
      friend class TlsConnection;
      SSL_CTX*       getTlsCtx ();
      SSL_CTX*       getSslCtx ();
};


class Security : public BaseSecurity
{
   public:
#ifdef WIN32
      Security( const Data& pathToCerts = "C:\\sipCerts\\");
#else
      Security( const Data& pathToCerts = "~/.sipCerts/" );
#endif

      virtual void preload();

      virtual void onReadPEM(const Data& name, PEMType type, Data& buffer) const;
      virtual void onWritePEM(const Data& name, PEMType type, const Data& buffer) const;
      virtual void onRemovePEM(const Data& name, PEMType type) const;
   private:
      Data mPath;
};

}

#endif

/* ====================================================================
 * The Vovida Software License, Version 1.0
 *
 * Copyright (c) 2000 Vovida Networks, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The names "VOCAL", "Vovida Open Communication Application Library",
 *    and "Vovida Open Communication Application Library (VOCAL)" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact vocal@vovida.org.
 *
 * 4. Products derived from this software may not be called "VOCAL", nor
 *    may "VOCAL" appear in their name, without prior written
 *    permission of Vovida Networks, Inc.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL VOVIDA
 * NETWORKS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT DAMAGES
 * IN EXCESS OF $1,000, NOR FOR ANY INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * ====================================================================
 *
 * This software consists of voluntary contributions made by Vovida
 * Networks, Inc. and many individuals on behalf of Vovida Networks,
 * Inc.  For more information on Vovida Networks, Inc., please see
 * <http://www.vovida.org/>.
 *
 */


