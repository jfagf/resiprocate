

#ifndef WIN32
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "sip2/util/Socket.hxx"
#include "sip2/util/Fifo.hxx"
#include "sip2/util/Data.hxx"
#include "sip2/util/Logger.hxx"
#include "sip2/util/Random.hxx"

#include "sip2/sipstack/SipStack.hxx"
#include "sip2/sipstack/Executive.hxx"
#include "sip2/sipstack/SipMessage.hxx"
#include "sip2/sipstack/Message.hxx"
#include "sip2/sipstack/Security.hxx"



#ifdef WIN32
#pragma warning( disable : 4355 )
#endif

using namespace Vocal2;

#define VOCAL_SUBSYSTEM Subsystem::SIP

SipStack::SipStack(bool multiThreaded)
   : 
#ifdef USE_SSL
   security( 0 ),
#endif
   mExecutive(*this),
   mTransportSelector(*this),
   mTimers(mStateMacFifo),
   mDnsResolver(*this)
{
   Random::initialize();
   initNetwork();

#ifdef USE_SSL
   security = new Security;
#endif

   //addTransport(Transport::UDP, 5060);
   //addTransport(Transport::TCP, 5060); // !jf!
}


void 
SipStack::addTransport( Transport::Type protocol, 
                        int port,
                        const Data& hostName,
                        const Data& nic) 
{
   mTransportSelector.addTransport(protocol, port, hostName, nic);
   if (!hostName.empty()) 
   {
      addAlias(hostName);
   }
}

void
SipStack::addAlias(const Data& domain)
{
   InfoLog (<< "Adding domain alias: " << domain);
   mDomains.insert(domain);
}

Data 
SipStack::getHostname() const
{
   // if you change this, please #def old version for windows 
   char hostName[1024];
   int err =  gethostname( hostName, sizeof(hostName) );
   assert( err == 0 );
   
   struct hostent* hostEnt = gethostbyname( hostName );
   assert( hostEnt );
   
   struct in_addr* addr = (struct in_addr*) hostEnt->h_addr_list[0];
   assert( addr );
   
   // if you change this, please #def old version for windows 
   char* addrA = inet_ntoa( *addr );
   Data ret(addrA);

   Data retHost( hostEnt->h_name );
      
   return retHost;
}

bool 
SipStack::isMyDomain(const Data& domain) const
{
   return (mDomains.count(domain) != 0);
}


void 
SipStack::send(const SipMessage& msg)
{
   InfoLog (<< "SipStack::Send: " << msg.brief());
   
   SipMessage* toSend = new SipMessage(msg);
   toSend->setFromTU();
   mStateMacFifo.add(toSend);
}


// this is only if you want to send to a destination not in the route. You
// probably don't want to use it. 
void 
SipStack::sendTo(const SipMessage& msg, const Uri& uri)
{
   SipMessage* toSend = new SipMessage(msg);
   toSend->setTarget(uri);
   toSend->setFromTU();
   mStateMacFifo.add(toSend);
}


SipMessage* 
SipStack::receive()
{
   // Check to see if a message is available and if it is return the 
   // waiting message. Otherwise, return 0
   if (mTUFifo.messageAvailable())
   {
      // we should only ever have SIP messages on the TU Fifo
      Message *tmpMsg = mTUFifo.getNext();
      SipMessage *sipMsg = dynamic_cast<SipMessage*>(tmpMsg);
      assert (sipMsg);
      return sipMsg;
   }
   else
   {
      return 0;
   }
}


void 
SipStack::process(FdSet& fdset)
{
   mExecutive.process(fdset);
}


/// returns time in milliseconds when process next needs to be called 
int 
SipStack::getTimeTillNextProcessMS()
{
   return mExecutive.getTimeTillNextProcessMS();
} 


void 
SipStack::buildFdSet(FdSet& fdset)
{
   mExecutive.buildFdSet( fdset );
}


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