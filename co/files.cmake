
# Copyright (c)      2010 Cedric Stalder <cedric.stalder@gmail.ch>
#               2011-2014 Stefan Eilemann <eile@eyescale.ch>
#                    2012 Daniel Nachbaur <danielnachbaur@gmail.com>
#                    2013 David Steiner <steiner@ifi.uzh.ch>

set(COLLAGE_PUBLIC_HEADERS
  ${COMMON_INCLUDES}
  barrier.h
  buffer.h
  bufferConnection.h
  bufferListener.h
  centLoadAwareDistributor.h
  commandFunc.h
  commandQueue.h
  commands.h
  connection.h
  connectionDescription.h
  connectionSet.h
  connectionType.h
  consumer.h
  customICommand.h
  customOCommand.h
  dataIStream.h
  dataIStream.ipp
  dataIStreamArchive.h
  dataIStreamArchive.ipp
  dataOStream.h
  dataOStream.ipp
  dataOStreamArchive.h
  dataOStreamArchive.ipp
  dataStreamArchiveException.h
  dispatcher.h
  equalDistributor.h
  packageDistributor.h
  exception.h
  features.h
  global.h
  iCommand.h
  init.h
  latencyAwareDistributor.h
  loadAwareDistributor.h
  localNode.h
  log.h
  node.h
  nodeType.h
  oCommand.h
  object.h
  objectFactory.h
  objectHandler.h
  objectICommand.h
  objectMap.h
  objectOCommand.h
  objectVersion.h
  perfLogger.h
  producer.h
  queueItem.h
  queueMaster.h
  queueSlave.h
  randomDistributor.h
  sendToken.h
  serializable.h
  slaveFeedback.h
  stealingQueueCommand.h
  stealingQueueMaster.h
  stealingQueueSlave.h
  types.h
  worker.h
  worker.ipp
  zeroconf.h
  )

set(COLLAGE_HEADERS
  barrierCommand.h
  bufferCache.h
  connectionListener.h
  dataIStreamQueue.h
  dataStreamArchive.h
  deltaMasterCM.h
  eventConnection.h
  fullMasterCM.h
  instanceCache.h
  masterCMCommand.h
  nodeCommand.h
  nullCM.h
  objectCM.h
  objectCommand.h
  objectDataICommand.h
  objectDataIStream.h
  objectDataOCommand.h
  objectDataOStream.h
  objectDeltaDataOStream.h
  objectInstanceDataOStream.h
  objectSlaveDataOStream.h
  objectStore.h
  pipeConnection.h
  queueCommand.h
  rspConnection.h
  socketConnection.h
  staticMasterCM.h
  staticSlaveCM.h
  unbufferedMasterCM.h
  versionedMasterCM.h
  versionedSlaveCM.h
  )

set(COLLAGE_SOURCES
  ${COMMON_SOURCES}
  barrier.cpp
  buffer.cpp
  bufferCache.cpp
  bufferConnection.cpp
  centLoadAwareDistributor.cpp
  commandQueue.cpp
  connection.cpp
  connectionDescription.cpp
  connectionSet.cpp
  customICommand.cpp
  customOCommand.cpp
  dataIStream.cpp
  dataIStreamArchive.cpp
  dataIStreamQueue.cpp
  dataOStream.cpp
  dataOStreamArchive.cpp
  deltaMasterCM.cpp
  dispatcher.cpp
  equalDistributor.cpp
  eventConnection.cpp
  fullMasterCM.cpp
  global.cpp
  iCommand.cpp
  init.cpp
  instanceCache.cpp
  latencyAwareDistributor.cpp
  loadAwareDistributor.cpp
  localNode.cpp
  masterCMCommand.cpp
  node.cpp
  oCommand.cpp
  object.cpp
  objectCM.cpp
  objectDataICommand.cpp
  objectDataIStream.cpp
  objectDataOCommand.cpp
  objectDataOStream.cpp
  objectDeltaDataOStream.cpp
  objectHandler.cpp
  objectICommand.cpp
  objectInstanceDataOStream.cpp
  objectMap.cpp
  objectOCommand.cpp
  objectSlaveDataOStream.cpp
  objectStore.cpp
  objectVersion.cpp
  pipeConnection.cpp
  queueItem.cpp
  queueMaster.cpp
  queueSlave.cpp
  randomDistributor.cpp
  rspConnection.cpp
  sendToken.cpp
  serializable.cpp
  slaveFeedback.cpp
  socketConnection.cpp
  staticSlaveCM.cpp
  stealingQueueMaster.cpp
  stealingQueueSlave.cpp
  unbufferedMasterCM.cpp
  versionedMasterCM.cpp
  versionedSlaveCM.cpp
  worker.cpp
  zeroconf.cpp
  )

if(WIN32)
  list(APPEND COLLAGE_HEADERS namedPipeConnection.h)
  list(APPEND COLLAGE_SOURCES namedPipeConnection.cpp)
else()
  list(APPEND COLLAGE_HEADERS fdConnection.h)
  list(APPEND COLLAGE_SOURCES fdConnection.cpp)
endif()

if(OFED_FOUND)
  list(APPEND COLLAGE_HEADERS rdmaConnection.h)
  list(APPEND COLLAGE_SOURCES rdmaConnection.cpp)
endif()

if(UDT_FOUND)
  list(APPEND COLLAGE_HEADERS udtConnection.h)
  list(APPEND COLLAGE_SOURCES udtConnection.cpp)
endif()

list(SORT COLLAGE_HEADERS)
list(SORT COLLAGE_SOURCES)
