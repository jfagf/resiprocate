#include "resiprocate/config.hxx"
#include "resiprocate/TuIM.hxx"
#include "GagMessage.hxx"
#include "GagConduit.hxx"
#include "resiprocate/os/Logger.hxx"

#define RESIPROCATE_SUBSYSTEM Subsystem::APP

GagConduit::GagConduit(SipStack &stack, int _udpPort)
  : sipStack(&stack), udpPort(_udpPort), running(true)
{
  // Nothing interesting to do here
}

void
GagConduit::handleMessage(GagMessage *message)
{
  switch (message->getMessageType())
  {
    case GagMessage::IM:
      gaimIm(reinterpret_cast<GagImMessage *>(message));
      break;
    case GagMessage::PRESENCE:
      gaimPresence(reinterpret_cast<GagPresenceMessage *>(message));
      break;
    case GagMessage::HELLO:
      gaimHello(reinterpret_cast<GagHelloMessage *>(message));
      break;
    case GagMessage::LOGIN:
      gaimLogin(reinterpret_cast<GagLoginMessage *>(message));
      break;
    case GagMessage::LOGOUT:
      gaimLogout(reinterpret_cast<GagLogoutMessage *>(message));
      break;
    case GagMessage::ADD_BUDDY:
      gaimAddBuddy(reinterpret_cast<GagAddBuddyMessage *>(message));
      break;
    case GagMessage::REMOVE_BUDDY:
      gaimRemoveBuddy(reinterpret_cast<GagRemoveBuddyMessage *>(message));
      break;
    case GagMessage::SHUTDOWN:
      gaimShutdown(reinterpret_cast<GagShutdownMessage *>(message));
      break;
    case GagMessage::GAG_ERROR:
      gaimError(reinterpret_cast<GagErrorMessage *>(message));
      break;
    case GagMessage::LOGIN_STATUS:
      gaimLoginStatus(reinterpret_cast<GagLoginStatusMessage *>(message));
      break;
    default:
      InfoLog ( << "Ignoring unexpected message of type " 
                << message->getMessageType());
      break;
  }
}


GagConduit::~GagConduit()
{
  removeAllUsers();
}

void
GagConduit::removeAllUsers()
{
  // Shut down all logged in users
  map<Uri,TuIM *>::iterator tu;
  tu = tuIM.begin();
  while (tu != tuIM.end())
  {
    delete(tu->second);
    tuIM.erase(tu);
    tu++;
  }
}

map<Uri,TuIM *>::iterator
GagConduit::getTu(Uri &aor)
{
  map<Uri,TuIM *>::iterator tu = tuIM.find(aor);
  if (tu == tuIM.end())
  {
    Data error;
    error = "You are not logged in as ";
    error += Data::from(aor);
    GagErrorMessage(error).serialize(cout);
  }
  return tu;
}

void
GagConduit::gaimIm(GagImMessage *msg)
{
  Uri *from = msg->getFromPtr();
  Uri *to = msg->getToPtr();
  Data *im = msg->getImPtr();

  map<Uri,TuIM *>::iterator tu = getTu(*from);
  if (tu == tuIM.end()) return;

  tu->second->sendPage(*im, *to, false, Data::Empty);
}

void
GagConduit::gaimPresence(GagPresenceMessage *msg)
{
  Uri *aor = msg->getAorPtr();
  bool online = msg->getAvailable();
  Data *status = msg->getStatusPtr();

  map<Uri,TuIM *>::iterator tu = getTu(*aor);
  if (tu == tuIM.end()) return;
  
  tu->second->setMyPresence(online, *status);
}

void
GagConduit::gaimHello(GagHelloMessage *msg)
{
  // We don't need to do anything when we get a hello.
  // Not yet, at least.
}

void
GagConduit::gaimLogin(GagLoginMessage *msg)
{
  Uri *aor = msg->getAorPtr();
  Data *userid = msg->getUseridPtr();
  Data *password = msg->getPasswordPtr();
  TuIM *newTu;


  map<Uri,TuIM *>::iterator tu = tuIM.find(*aor);
  if (tu != tuIM.end())
  {
    Data error;
    error = "You are already logged in as ";
    error += Data::from(aor);
    GagErrorMessage(error).serialize(cout);
  }

  // Figure out what out contact is
  Uri contact;

/* Were going to leave all of this stuff to the stack now - RjS
  contact.port() = udpPort;

#ifdef WIN32
  contact.host() = sipStack->getHostAddress();
#else
  {
    char *contactHostOverride = getenv("GAGHOSTNAME");
    if (contactHostOverride)
    {
      contact.host() = Data(contactHostOverride);
    }
    else
    {
      contact.host() = sipStack->getHostAddress();
    }
  }
#endif
*/

  contact.user() = aor->user();

  newTu = new TuIM(sipStack, *aor, contact, this );
  if (!newTu)
  {
    Data error;
    error = "Ran out of memory when logging in as ";
    error += Data::from(aor);
    GagErrorMessage(error).serialize(cout);
    return;
  }
  newTu->setUAName(Data("gag/0.0.1 (gaim)"));
  newTu->registerAor(*aor, *password);

  tuIM[*aor] = newTu;
}

void
GagConduit::gaimLogout(GagLogoutMessage *msg)
{
  Uri *aor = msg->getAorPtr();
  map<Uri,TuIM *>::iterator tu = getTu(*aor);
  if (tu == tuIM.end()) return;
  
  delete(tu->second);
  tuIM.erase(tu);
}

void
GagConduit::gaimAddBuddy(GagAddBuddyMessage *msg)
{
  Uri *us = msg->getUsPtr();
  Uri *them = msg->getThemPtr();

  map<Uri,TuIM *>::iterator tu = getTu(*us);
  if (tu == tuIM.end()) return;

  tu->second->addBuddy(*them, Data::Empty);
}

void
GagConduit::gaimRemoveBuddy(GagRemoveBuddyMessage *msg)
{
  Uri *us = msg->getUsPtr();
  Uri *them = msg->getThemPtr();

  map<Uri,TuIM *>::iterator tu = getTu(*us);
  if (tu == tuIM.end()) return;

  tu->second->removeBuddy(*them);
}

void
GagConduit::gaimShutdown(GagShutdownMessage *msg)
{
  running = false;
  removeAllUsers();
}

void
GagConduit::gaimError(GagErrorMessage *msg)
{
  // This should *never* be called..
  InfoLog ( << "GAIM should not send me errors.");
}

void
GagConduit::gaimLoginStatus(GagLoginStatusMessage *msg)
{
  // This should *never* be called..
  InfoLog ( << "GAIM should not send me login status messages.");
}

void
GagConduit::process()
{
  map<Uri,TuIM *>::iterator tu;
  tu = tuIM.begin();
  while (tu != tuIM.end())
  {
    (tu->second)->process();
    tu++;
  }
}

void 
GagConduit::presenceUpdate(const Uri& dest, bool open,
                           const Data& status )
{
  InfoLog ( << " Gag got a presenceUpdate Callback");
  GagPresenceMessage(dest, open, status).serialize(cout);
}

void 
GagConduit::receivedPage( const Data& msg, const Uri& from ,
                          const Data& signedBy,
                          Security::SignatureStatus sigStatus,
                          bool wasEncryped)
{
  Uri to("sip:dummy@dummy.xx"); // XXX
  GagImMessage (from, to, msg).serialize(cout);
}

void 
GagConduit::sendPageFailed( const Uri& dest,int respNumber )
{
  Data error;
  error = "Could not send IM to ";
  error += Data::from(dest);
  error += " (";
  error += Data::from(respNumber);
  error += ")";

  GagErrorMessage (error).serialize(cout);
}

void 
GagConduit::registrationFailed(const resip::Uri& uri, int respNumber)
{
  Data error;
  error = "Could not register as ";
  error += Data::from(uri);
  error += " (";
  error += Data::from(respNumber);
  error += ")";

  GagLoginStatusMessage (false, respNumber, error).serialize(cout);
}

void 
GagConduit::registrationWorked(const Uri& dest )
{
  Data ok("Okay");
  GagLoginStatusMessage (true, 200, ok).serialize(cout);
}

void 
GagConduit::receivePageFailed(const Uri& sender)
{
  Data error;
  error = "Could not get IM from ";
  error += Data::from(sender);

  GagErrorMessage (error).serialize(cout);
}