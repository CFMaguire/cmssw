// $Id: ConsumerUtils.cc,v 1.8 2010/02/16 10:49:36 mommsen Exp $
/// @file: ConsumerUtils.cc

#include "EventFilter/StorageManager/interface/ConsumerID.h"
#include "EventFilter/StorageManager/interface/ConsumerUtils.h"
#include "EventFilter/StorageManager/interface/Exception.h"
#include "EventFilter/StorageManager/interface/I2OChain.h"
#include "EventFilter/StorageManager/interface/StatisticsReporter.h"

#include "IOPool/Streamer/interface/ConsRegMessage.h"
#include "IOPool/Streamer/interface/OtherMessage.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/EDMException.h"

#include "xcept/tools.h"
#include "xgi/Input.h"
#include "xgi/Output.h"
#include "xgi/exception/Exception.h"

#include <string>
#include <vector>
#include <memory>

using namespace stor;



void ConsumerUtils::processConsumerRegistrationRequest(xgi::Input* in, xgi::Output* out) const
{
  // Get consumer ID if registration is allowed:
  ConsumerID cid = _sharedResources->_registrationCollection->getConsumerID();

  if( !cid.isValid() )
  {
    writeNotReady( out );
    return;
  }

  // Create registration info and set consumer ID:
  ConsRegPtr reginfo;
  std::string errorMsg = "Error parsing an event consumer registration request";
  StatisticsReporter::AlarmHandlerPtr alarmHandler =
    _sharedResources->_statisticsReporter->alarmHandler();
  try
  {
    reginfo = parseEventConsumerRegistration(in);
  }
  catch ( edm::Exception& excpt )
  {
    errorMsg.append( ": " );
    errorMsg.append( excpt.what() );
    
    XCEPT_DECLARE(stor::exception::ConsumerRegistration,
      sentinelException, errorMsg);
    alarmHandler->notifySentinel(AlarmHandler::ERROR, sentinelException);
    
    writeErrorString( out, errorMsg );
    return;
  }
  catch ( xcept::Exception& excpt )
  {
    XCEPT_DECLARE_NESTED(stor::exception::ConsumerRegistration,
      sentinelException, errorMsg, excpt);
    alarmHandler->notifySentinel(AlarmHandler::ERROR, sentinelException);
    
    writeErrorString( out, errorMsg + ": " + xcept::stdformat_exception_history(excpt) );
    return;
  }
  catch ( ... )
  {
    errorMsg.append( ": unknown exception" );
    
    XCEPT_DECLARE(stor::exception::ConsumerRegistration,
      sentinelException, errorMsg);
    alarmHandler->notifySentinel(AlarmHandler::ERROR, sentinelException);
    
    writeErrorString( out, errorMsg );
    return;
  }
  reginfo->setConsumerID( cid );

  // Create queue and set queue ID:
  QueueID qid =
    _sharedResources->_eventConsumerQueueCollection->createQueue(
      cid,
      reginfo->queuePolicy(),
      reginfo->queueSize(),
      reginfo->secondsToStale());

  if( !qid.isValid() )
  {
    writeNotReady( out );
    return;
  }
  
  reginfo->setQueueID( qid );
  
  // Add registration to collection:
  bool add_ok = 
    _sharedResources->_registrationCollection->addRegistrationInfo( cid,
                                                                    reginfo );
  if( !add_ok )
  {
    writeNotReady( out );
    return;
  }

  // Put registration on the queue:
  _sharedResources->_registrationQueue->enq_wait( reginfo );
  
  // Reply to consumer:
  writeConsumerRegistration( out, cid );
}


void ConsumerUtils::processConsumerHeaderRequest(xgi::Input* in, xgi::Output* out) const
{
  ConsumerID cid = getConsumerID( in );
  if( !cid.isValid() )
  {
    writeEmptyBuffer( out );
    return;
  }
  
  ConsRegPtr consRegPtr = boost::dynamic_pointer_cast<EventConsumerRegistrationInfo>(
    _sharedResources->_registrationCollection->getRegistrationInfo( cid ));
  if ( consRegPtr.get() == NULL )
  {
    writeEmptyBuffer( out );
    return;
  }
  
  InitMsgSharedPtr payload =
    _sharedResources->_initMsgCollection->getElementForOutputModule( consRegPtr->outputModuleLabel() );
  
  if( payload.get() == NULL )
  {
    writeEmptyBuffer( out );
    return;
  }
  
  writeConsumerHeader( out, payload );
}


void ConsumerUtils::processConsumerEventRequest(xgi::Input* in, xgi::Output* out) const
{
  ConsumerID cid = getConsumerID( in );
  if( !cid.isValid() )
  {
    writeEmptyBuffer( out );
    return;
  }

  if ( !_sharedResources->_registrationCollection->registrationIsAllowed() )
  {
    writeDone( out );
    return;
  }

  I2OChain evt =
    _sharedResources->_eventConsumerQueueCollection->popEvent( cid );
  if( evt.faulty() )
  {
    writeEmptyBuffer( out );
    return;
  }

  writeConsumerEvent( out, evt );
}


void ConsumerUtils::processDQMConsumerRegistrationRequest(xgi::Input* in, xgi::Output* out) const
{
  // Get consumer ID if registration is allowed:
  ConsumerID cid = _sharedResources->_registrationCollection->getConsumerID();
  
  if( !cid.isValid() )
  {
    writeNotReady( out );
    return;
  }

  // Create registration info and set consumer ID:
  stor::DQMEventConsRegPtr dqmreginfo;
  std::string errorMsg = "Error parsing a DQM event consumer registration request";
  StatisticsReporter::AlarmHandlerPtr alarmHandler =
    _sharedResources->_statisticsReporter->alarmHandler();
  try
  {
    dqmreginfo = parseDQMEventConsumerRegistration(in);
  }
  catch ( edm::Exception& excpt )
  {
    errorMsg.append( ": " );
    errorMsg.append( excpt.what() );
    
    XCEPT_DECLARE(stor::exception::DQMConsumerRegistration,
      sentinelException, errorMsg);
    alarmHandler->notifySentinel(AlarmHandler::ERROR, sentinelException);
    
    writeErrorString( out, errorMsg );
    return;
  }
  catch ( xcept::Exception& excpt )
  {
    XCEPT_DECLARE_NESTED(stor::exception::DQMConsumerRegistration,
      sentinelException, errorMsg, excpt);
    alarmHandler->notifySentinel(AlarmHandler::ERROR, sentinelException);
    
    writeErrorString( out, errorMsg + ": " + xcept::stdformat_exception_history(excpt) );
    return;
  }
  catch ( ... )
  {
    errorMsg.append( ": unknown exception" );
    
    XCEPT_DECLARE(stor::exception::DQMConsumerRegistration,
      sentinelException, errorMsg);
    alarmHandler->notifySentinel(AlarmHandler::ERROR, sentinelException);
    
    writeErrorString( out, errorMsg );
    return;
  }
  dqmreginfo->setConsumerID( cid );

  // Create queue and set queue ID:
  QueueID qid =
    _sharedResources->_dqmEventConsumerQueueCollection->createQueue(
      cid,
      dqmreginfo->queuePolicy(),
      dqmreginfo->queueSize(),
      dqmreginfo->secondsToStale());

  if( !qid.isValid() )
  {
    writeNotReady( out );
    return;
  }
  
  dqmreginfo->setQueueID( qid );

  // Add registration to collection:
  bool add_ok = 
    _sharedResources->_registrationCollection->addRegistrationInfo( cid,
                                                                    dqmreginfo );
  if( !add_ok )
  {
    writeNotReady( out );
    return;
  }

  // Put registration on the queue:
  _sharedResources->_registrationQueue->enq_wait( dqmreginfo );

  // Reply to consumer:
  writeConsumerRegistration( out, cid );
}


void ConsumerUtils::processDQMConsumerEventRequest(xgi::Input* in, xgi::Output* out) const
{
  ConsumerID cid = getConsumerID( in );
  if( !cid.isValid() )
  {
    writeEmptyBuffer( out );
    return;
  }

  if ( !_sharedResources->_registrationCollection->registrationIsAllowed() )
  {
    writeDone( out );
    return;
  }
  
  DQMEventRecord::GroupRecord dqmGroupRecord =
    _sharedResources->_dqmEventConsumerQueueCollection->popEvent( cid );

  if ( !dqmGroupRecord.empty() )
    writeDQMConsumerEvent( out, dqmGroupRecord.getDQMEventMsgView() );
  else
    writeEmptyBuffer( out );
}


////////////////////////////////////////////
//// Create consumer registration info: ////
////////////////////////////////////////////
ConsRegPtr ConsumerUtils::parseEventConsumerRegistration(xgi::Input* in) const
{
  
  if( in == 0 )
  {
    XCEPT_RAISE( xgi::exception::Exception,
      "Null xgi::Input* in parseEventConsumerRegistration" );
  }
    
  const utils::duration_t secondsToStale =
    _sharedResources->_configuration->getEventServingParams()._activeConsumerTimeout;
  const enquing_policy::PolicyTag queuePolicy = enquing_policy::DiscardOld;
  const int queueSize =
    _sharedResources->_configuration->getEventServingParams()._consumerQueueSize;

  std::string name = "unknown";
  std::string pset_str = "<>";
  
  const std::string l_str = in->getenv( "CONTENT_LENGTH" );
  unsigned long l = std::atol( l_str.c_str() );
  
  const std::string remote_host = in->getenv( "REMOTE_HOST" );

  if( l > 0 )
  {
    std::auto_ptr< std::vector<char> > buf( new std::vector<char>(l) );
    in->read( &(*buf)[0], l );
    ConsRegRequestView req( &(*buf)[0] );
    name = req.getConsumerName();
    pset_str = req.getRequestParameterSet();
  }
  else
  {
    XCEPT_RAISE( stor::exception::ConsumerRegistration,
      "Bad request length" );
  }

  const edm::ParameterSet pset( pset_str );
  
  //
  //// Check if HLT output module is there: ////
  //
  
  std::string sel_hlt_out = "";
  
  try
  {
    // new-style consumer
    sel_hlt_out = pset.getParameter<std::string>( "TrackedHLTOutMod" );
  }
  catch( edm::Exception& e )
  {
    // old-style consumer or param not specified
    sel_hlt_out =
      pset.getUntrackedParameter<std::string>( "SelectHLTOutput", "" );
  }
  
  if( sel_hlt_out == "" )
  {
    XCEPT_RAISE( stor::exception::ConsumerRegistration,
      "No HLT output module specified" );
  }
  
  // Event filters:
  std::string sel_events_new = std::string();
  try
  {
    sel_events_new = pset.getParameter<std::string>("TriggerSelector");
  }
  catch (edm::Exception& e) {}
  
  EventConsumerRegistrationInfo::FilterList sel_events;
  try
  {
    sel_events = pset.getParameter<Strings>( "TrackedEventSelection" );
  }
  catch( edm::Exception& e )
  {
    edm::ParameterSet tmpPSet1 =
      pset.getUntrackedParameter<edm::ParameterSet>( "SelectEvents",
        edm::ParameterSet() );
    if ( ! tmpPSet1.empty() )
    {
      sel_events = tmpPSet1.getParameter<Strings>( "SelectEvents" );
    }
  }
  
  // Number of retries:
  unsigned int max_conn_retr = 5;
  try
  {
    max_conn_retr = pset.getParameter<int>( "TrackedMaxConnectTries" );
  }
  catch( edm::Exception& e )
  {
    pset.getUntrackedParameter<int>( "maxConnectTries", 5 );
  }
  
  // Retry interval:
  unsigned int conn_retr_interval = 10;
  try
  {
    conn_retr_interval =
      pset.getParameter<int>( "TrackedConnectTrySleepTime" );
  }
  catch( edm::Exception& e )
  {
    conn_retr_interval =
      pset.getUntrackedParameter<int>( "connectTrySleepTime", 10 );
  }
  
  ConsRegPtr cr( new EventConsumerRegistrationInfo( max_conn_retr,
                                                    conn_retr_interval,
                                                    name,
                                                    sel_events_new,
                                                    sel_events,
                                                    sel_hlt_out,
                                                    queueSize,
                                                    queuePolicy,
                                                    secondsToStale,
                                                    remote_host ) );
  return cr;
}

////////////////////////////////////////////////
//// Create DQM consumer registration info: ////
////////////////////////////////////////////////
DQMEventConsRegPtr ConsumerUtils::parseDQMEventConsumerRegistration(xgi::Input* in) const
{
  if( in == 0 )
  {
    XCEPT_RAISE( xgi::exception::Exception,
      "Null xgi::Input* in parseDQMEventConsumerRegistration" );
  }
  
  const utils::duration_t secondsToStale =
    _sharedResources->_configuration->getEventServingParams()._DQMactiveConsumerTimeout;
  const enquing_policy::PolicyTag queuePolicy = enquing_policy::DiscardOld;
  const int queueSize =
    _sharedResources->_configuration->getEventServingParams()._DQMconsumerQueueSize;

  std::string consumerName = "None provided";
  std::string consumerTopFolderName = "*";
  
  // read the consumer registration message from the http input stream
  std::string lengthString = in->getenv("CONTENT_LENGTH");
  unsigned int contentLength = std::atol(lengthString.c_str());
  if (contentLength > 0)
  {
    std::auto_ptr< std::vector<char> > bufPtr(new std::vector<char>(contentLength));
    in->read(&(*bufPtr)[0], contentLength);
    ConsRegRequestView requestMessage(&(*bufPtr)[0]);
    consumerName = requestMessage.getConsumerName();
    // for DQM consumers top folder name is stored in the "parameteSet"
    std::string reqFolder = requestMessage.getRequestParameterSet();
    if (reqFolder.size() >= 1) consumerTopFolderName = reqFolder;
  }
  
  const std::string remote_host = in->getenv( "REMOTE_HOST" );
  
  DQMEventConsRegPtr cr( new DQMEventConsumerRegistrationInfo( consumerName,
                                                               consumerTopFolderName, 
                                                               queueSize,
                                                               queuePolicy,
                                                               secondsToStale,
                                                               remote_host ) );
  return cr;
}

/////////////////////////////
//// Write HTTP headers: ////
/////////////////////////////
void ConsumerUtils::writeHTTPHeaders(xgi::Output* out) const
{
  out->getHTTPResponseHeader().addHeader( "Content-Type",
                                          "application/octet-stream" );
  out->getHTTPResponseHeader().addHeader( "Content-Transfer-Encoding",
                                          "binary" );
}

//////////////////////////////
//// Send ID to consumer: ////
//////////////////////////////
void ConsumerUtils::writeConsumerRegistration(xgi::Output* out, const ConsumerID cid) const
{

  const int buff_size = 1000;
  std::vector<unsigned char> buff( buff_size );

  ConsRegResponseBuilder rb( &buff[0], buff.capacity(), 0, cid.value );
  ConsRegResponseView rv( &buff[0] );
  const unsigned int len = rv.size();

  writeHTTPHeaders( out );
  out->write( (char*)(&buff[0]), len );

}

////////////////////////////////////////
//// Tell consumer we're not ready: ////
////////////////////////////////////////
void ConsumerUtils::writeNotReady(xgi::Output* out) const
{

  const int buff_size = 1000;
  std::vector<unsigned char> buff( buff_size );

  ConsRegResponseBuilder rb( &buff[0], buff.capacity(),
                             ConsRegResponseBuilder::ES_NOT_READY, 0 );
  ConsRegResponseView rv( &buff[0] );
  const unsigned int len = rv.size();

  writeHTTPHeaders( out );
  out->write( (char*)(&buff[0]), len );

}

////////////////////////////////////////
//// Send empty buffer to consumer: ////
////////////////////////////////////////
void ConsumerUtils::writeEmptyBuffer(xgi::Output* out) const
{
  char buff;
  writeHTTPHeaders( out );
  out->write( &buff, 0 );
}

///////////////////////////////////////////
//// Tell consumer that run has ended: ////
///////////////////////////////////////////
void ConsumerUtils::writeDone(xgi::Output* out) const
{

  const int buff_size = 1000;
  std::vector<unsigned char> buff( buff_size );

  OtherMessageBuilder omb( &buff[0], Header::DONE );
  const unsigned int len = omb.size();

  writeHTTPHeaders( out );
  out->write( (char*)(&buff[0]), len );

}

/////////////////////////////////////////
//// Send error message to consumer: ////
/////////////////////////////////////////
void ConsumerUtils::writeErrorString(xgi::Output* out, const std::string errorString) const
{

  const int buff_size = errorString.size();
  std::vector<unsigned char> buff( buff_size );

  const char *errorBytes = errorString.c_str();
  for (int i=0; i<buff_size; ++i) buff[i]=errorBytes[i];

  writeHTTPHeaders( out );
  out->write( (char*)(&buff[0]), buff_size );

}


//////////////////////////////////////////////////
//// Extract consumer ID from header request: ////
//////////////////////////////////////////////////
ConsumerID ConsumerUtils::getConsumerID(xgi::Input* in) const
{

  if( in == 0 )
  {          
    XCEPT_RAISE( xgi::exception::Exception,
      "Null xgi::Input* in getConsumerID" );
  }
  
  const std::string l_str = in->getenv( "CONTENT_LENGTH" );
  unsigned long l = std::atol( l_str.c_str() );
  
  unsigned int cid_int = 0;
  
  if( l > 0 )
  {
    std::auto_ptr< std::vector<char> > buf( new std::vector<char>(l) );
    in->read( &(*buf)[0], l );
    OtherMessageView req( &(*buf)[0] );
    if( req.code() == Header::HEADER_REQUEST ||
        req.code() == Header::EVENT_REQUEST ||
        req.code() == Header::DQMEVENT_REQUEST )
    {
      uint8* ptr = req.msgBody();
      cid_int = convert32( ptr );
    }
    else
    {
      XCEPT_RAISE( stor::exception::Exception,
        "Bad request code in getConsumerID" );
    }
  }
  else
  {
    XCEPT_RAISE( stor::exception::Exception,
      "Bad request length in getConsumerID" );
  }
  
  return ConsumerID( cid_int );
}

///////////////////////
//// Write header: ////
///////////////////////
void ConsumerUtils::writeConsumerHeader(xgi::Output* out, const InitMsgSharedPtr ptr) const
{
  const unsigned int len = ptr->size();
  std::vector<unsigned char> buff( len );
  for( unsigned int i = 0; i < len; ++i )
  {
    buff[i] = (*ptr)[i];
  }
  writeHTTPHeaders( out );
  out->write( (char*)(&buff[0]), len );
}

//////////////////////
//// Write event: ////
//////////////////////
void ConsumerUtils::writeConsumerEvent(xgi::Output* out, const I2OChain& evt) const
{
  writeHTTPHeaders( out );

  const unsigned int nfrags = evt.fragmentCount();
  for ( unsigned int i = 0; i < nfrags; ++i )
  {
    const unsigned int len = evt.dataSize( i );
    unsigned char* location = evt.dataLocation( i );
    out->write( (char*)location, len );
  } 
}

//////////////////////////
//// Write DQM event: ////
//////////////////////////
void ConsumerUtils::writeDQMConsumerEvent(xgi::Output* out, const DQMEventMsgView& view) const
{
  writeHTTPHeaders( out );

  const unsigned int len = view.size();
  unsigned char* location = view.startAddress();
  out->write( (char*)location, len );
}


/// emacs configuration
/// Local Variables: -
/// mode: c++ -
/// c-basic-offset: 2 -
/// indent-tabs-mode: nil -
/// End: -
