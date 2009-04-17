/*
 TUIO C++ Library - part of the reacTIVision project
 http://reactivision.sourceforge.net/
 
 Copyright (c) 2005-2009 Martin Kaltenbrunner <mkalten@iua.upf.edu>
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "TuioClient.h"
#include <time.h>

using namespace TUIO;

#ifndef WIN32
static void* ClientThreadFunc( void* obj )
#else
static DWORD WINAPI ClientThreadFunc( LPVOID obj )
#endif
{
	static_cast<TuioClient*>(obj)->socket->Run();
	return 0;
};

void TuioClient::lockObjectList() {
	if(!connected) return;
#ifndef WIN32	
	pthread_mutex_lock(&objectMutex);
#else
	WaitForSingleObject(objectMutex, INFINITE);
#endif		
}

void TuioClient::unlockObjectList() {
	if(!connected) return;
#ifndef WIN32	
	pthread_mutex_unlock(&objectMutex);
#else
	ReleaseMutex(objectMutex);
#endif
}

void TuioClient::lockCursorList() {
	if(!connected) return;
#ifndef WIN32	
	pthread_mutex_lock(&cursorMutex);
#else
	WaitForSingleObject(cursorMutex, INFINITE);
#endif		
}

void TuioClient::unlockCursorList() {
	if(!connected) return;
#ifndef WIN32	
	pthread_mutex_unlock(&cursorMutex);
#else
	ReleaseMutex(cursorMutex);
#endif		
}

TuioClient::TuioClient(int port) {
	try {
		socket = new UdpListeningReceiveSocket(IpEndpointName( IpEndpointName::ANY_ADDRESS, port ), this );
	} catch (std::exception &e) { 
		std::cerr << "could not bind to UDP port " << port << std::endl;
		socket = NULL;
	}
	
	if (socket!=NULL) {
		if (!socket->IsBound()) {
			delete socket;
			socket = NULL;
		} else std::cout << "listening to TUIO messages on UDP port " << port << std::endl;
	}
	
	locked = false;
	connected = false;
	currentFrame = maxCursorID = -1;
}

TuioClient::~TuioClient() {	
	delete socket;
}

void TuioClient::ProcessBundle( const ReceivedBundle& b, const IpEndpointName& remoteEndpoint) {
	
	try {
		for( ReceivedBundle::const_iterator i = b.ElementsBegin(); i != b.ElementsEnd(); ++i ){
			if( i->IsBundle() )
				ProcessBundle( ReceivedBundle(*i), remoteEndpoint);
			else
				ProcessMessage( ReceivedMessage(*i), remoteEndpoint);
		}
	} catch (MalformedBundleException& e) {
		std::cerr << "malformed OSC bundle" << std::endl << e.what() << std::endl;
	}
	
}

void TuioClient::ProcessMessage( const ReceivedMessage& msg, const IpEndpointName& remoteEndpoint) {
	try {
		ReceivedMessageArgumentStream args = msg.ArgumentStream();
		ReceivedMessage::const_iterator arg = msg.ArgumentsBegin();
		
		if( strcmp( msg.AddressPattern(), "/tuio/2Dobj" ) == 0 ){
			
			const char* cmd;
			args >> cmd;
			
			if (strcmp(cmd,"set")==0) {	
				
				if (currentTime.getTotalMilliseconds()==0)
					currentTime = TuioTime::getSessionTime();
								
				int32 s_id, c_id;
				float xpos, ypos, angle, xspeed, yspeed, rspeed, maccel, raccel;
				args >> s_id >> c_id >> xpos >> ypos >> angle >> xspeed >> yspeed >> rspeed >> maccel >> raccel;
				
				lockObjectList();
				std::list<TuioObject*>::iterator tobj;
				for (tobj=objectList.begin(); tobj!= objectList.end(); tobj++)
					if((*tobj)->getSessionID()==(long)s_id) break;
				
				if (tobj == objectList.end()) {
					TuioObject *addObject = new TuioObject(currentTime,(long)s_id,(int)c_id,xpos,ypos,angle);
					objectList.push_back(addObject);
					
					for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++)
						(*listener)->addTuioObject(addObject);
					
				} else if ( ((*tobj)->getX()!=xpos) || ((*tobj)->getY()!=ypos) || ((*tobj)->getAngle()!=angle) || ((*tobj)->getXSpeed()!=xspeed) || ((*tobj)->getYSpeed()!=yspeed) || ((*tobj)->getRotationSpeed()!=rspeed) || ((*tobj)->getMotionAccel()!=maccel) || ((*tobj)->getRotationAccel()!=raccel) ) {

					TuioObject *updateObject = new TuioObject(currentTime,(long)s_id,(*tobj)->getSymbolID(),xpos,ypos,angle);
					updateObject->update(currentTime,xpos,ypos,angle,xspeed,yspeed,rspeed,maccel,raccel);
					frameObjects.push_back(updateObject);
				
					/*(*tobj)->update(currentTime,xpos,ypos,angle,xspeed,yspeed,rspeed,maccel,raccel);
					for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++)
						(*listener)->updateTuioObject((*tobj));*/
				}
				unlockObjectList();
			} else if (strcmp(cmd,"alive")==0) {
				
				int32 s_id;
				while(!args.Eos()) {
					args >> s_id;
					objectBuffer.push_back((long)s_id);
					
					std::list<long>::iterator iter;
					iter = find(aliveObjectList.begin(), aliveObjectList.end(), (long)s_id); 
					if (iter != aliveObjectList.end()) aliveObjectList.erase(iter);
				}
				
				std::list<long>::iterator alive_iter;
				for (alive_iter=aliveObjectList.begin(); alive_iter != aliveObjectList.end(); alive_iter++) {
					lockObjectList();
					std::list<TuioObject*>::iterator tobj;
					for (tobj=objectList.begin(); tobj!=objectList.end(); tobj++) {
						TuioObject *deleteObject = (*tobj);
						if(deleteObject->getSessionID()==*alive_iter) {
							objectList.erase(tobj);
							deleteObject->remove(currentTime);
							for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++)
								(*listener)->removeTuioObject(deleteObject);
							delete deleteObject;
							break;
						}
					}
					unlockObjectList();
				}
				aliveObjectList = objectBuffer;
				objectBuffer.clear();
			} else if (strcmp(cmd,"fseq")==0) {
				
				int32 fseq;
				args >> fseq;
				bool lateFrame = false;
				if (fseq>0) {
					if ((fseq>=currentFrame) || ((currentFrame-fseq)>100)) currentFrame = fseq;
					else lateFrame = true;
				}
			
				if (!lateFrame) {
					
					for (std::list<TuioObject*>::iterator iter=frameObjects.begin(); iter != frameObjects.end(); iter++) {
						TuioObject *tobj = (*iter);
						TuioObject *updateObject = getTuioObject(tobj->getSessionID());
						updateObject->update(currentTime,tobj->getX(),tobj->getY(),tobj->getAngle(),tobj->getXSpeed(),tobj->getYSpeed(),tobj->getRotationSpeed(),tobj->getMotionAccel(),tobj->getRotationAccel());
						
						for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++)
							(*listener)->updateTuioObject(updateObject);
						
						delete tobj;
					}
					
					for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++)
						(*listener)->refresh(currentTime);
					
					if (fseq>0) currentTime.reset();
				} else {
					for (std::list<TuioObject*>::iterator iter=frameObjects.begin(); iter != frameObjects.end(); iter++) {
						TuioObject *tobj = (*iter);
						delete tobj;
					}
				}
				
				frameObjects.clear();
				
				/*TuioTime ftime = TuioTime::getSystemTime();
				long ft = ftime.getSeconds()*1000000 + ftime.getMicroseconds();
				if (currentFrame>0) std:: cout << "received frame " << currentFrame << " " << ft  << std::endl;*/
			}
		} else if( strcmp( msg.AddressPattern(), "/tuio/2Dcur" ) == 0 ) {
			const char* cmd;
			args >> cmd;
			
			if (strcmp(cmd,"set")==0) {	

				if (currentTime.getTotalMilliseconds()==0)
					currentTime = TuioTime::getSessionTime();

				int32 s_id;
				float xpos, ypos, xspeed, yspeed, maccel;				
				args >> s_id >> xpos >> ypos >> xspeed >> yspeed >> maccel;
				
				lockCursorList();
				std::list<TuioCursor*>::iterator tcur;
				for (tcur=cursorList.begin(); tcur!= cursorList.end(); tcur++)
					if((*tcur)->getSessionID()==(long)s_id) break;
				
				if (tcur==cursorList.end()) {
					
					int c_id = (int)cursorList.size();
					if ((int)(cursorList.size())<=maxCursorID) {
						std::list<TuioCursor*>::iterator closestCursor = freeCursorList.begin();
						
						for(std::list<TuioCursor*>::iterator iter = freeCursorList.begin();iter!= freeCursorList.end(); iter++) {
							if((*iter)->getDistance(xpos,ypos)<(*closestCursor)->getDistance(xpos,ypos)) closestCursor = iter;
						}
						
						TuioCursor *freeCursor = (*closestCursor);
						c_id = freeCursor->getCursorID();
						freeCursorList.erase(closestCursor);
						delete freeCursor;
					} else maxCursorID = c_id;	
					
					TuioCursor *addCursor = new TuioCursor(currentTime,(long)s_id,c_id,xpos,ypos);
					
					cursorList.push_back(addCursor);
					for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++)
						(*listener)->addTuioCursor(addCursor);
					
				} else if ( ((*tcur)->getX()!=xpos) || ((*tcur)->getY()!=ypos) || ((*tcur)->getXSpeed()!=xspeed) || ((*tcur)->getYSpeed()!=yspeed) || ((*tcur)->getMotionAccel()!=maccel) ) {

					TuioCursor *updateCursor = new TuioCursor(currentTime,(long)s_id,(*tcur)->getCursorID(),xpos,ypos);
					updateCursor->update(currentTime,xpos,ypos,xspeed,yspeed,maccel);
					frameCursors.push_back(updateCursor);

					/*(*tcur)->update(currentTime,xpos,ypos,xspeed,yspeed,maccel);
					for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++)
						(*listener)->updateTuioCursor((*tcur));*/
				}
				unlockCursorList();
				
			} else if (strcmp(cmd,"alive")==0) {
				
				int32 s_id;
				while(!args.Eos()) {
					args >> s_id;
					cursorBuffer.push_back((long)s_id);
					
					std::list<long>::iterator iter;
					iter = find(aliveCursorList.begin(), aliveCursorList.end(), (long)s_id); 
					if (iter != aliveCursorList.end()) aliveCursorList.erase(iter);
				}
				
				std::list<long>::iterator alive_iter;
				for (alive_iter=aliveCursorList.begin(); alive_iter != aliveCursorList.end(); alive_iter++) {
					lockCursorList();
					std::list<TuioCursor*>::iterator tcur;
					for (tcur=cursorList.begin(); tcur != cursorList.end(); tcur++) {
						TuioCursor *deleteCursor = (*tcur);
						if(deleteCursor->getSessionID()==*alive_iter) {
							
							cursorList.erase(tcur);
							deleteCursor->remove(currentTime);
							for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++)
								(*listener)->removeTuioCursor(deleteCursor);
							
							if (deleteCursor->getCursorID()==maxCursorID) {
								maxCursorID = -1;
								delete deleteCursor;
								
								if (!cursorList.empty()) {
									std::list<TuioCursor*>::iterator clist;
									for (clist=cursorList.begin(); clist != cursorList.end(); clist++) {
										int c_id = (*clist)->getCursorID();
										if (c_id>maxCursorID) maxCursorID=c_id;
									}
									
									freeCursorBuffer.clear();
									for (std::list<TuioCursor*>::iterator flist=freeCursorList.begin(); flist != freeCursorList.end(); flist++) {
										TuioCursor *freeCursor = (*flist);
										if (freeCursor->getCursorID()>maxCursorID) delete freeCursor;
										else freeCursorBuffer.push_back(freeCursor);
									}	
									freeCursorList = freeCursorBuffer;
									
								} 
							} else if (deleteCursor->getCursorID()<maxCursorID) {
								freeCursorList.push_back(deleteCursor);
							}
							
							break;
						}
					}
					unlockCursorList();
				}
				
				aliveCursorList = cursorBuffer;
				cursorBuffer.clear();
			} else if( strcmp( cmd, "fseq" ) == 0 ){
				
				int32 fseq;
				args >> fseq;
				bool lateFrame = false;
				if (fseq>0) {
					if ((fseq>=currentFrame) || ((currentFrame-fseq)>100)) currentFrame = fseq;
					else lateFrame = true;
				}
			
				if (!lateFrame) {
					
					for (std::list<TuioCursor*>::iterator iter=frameCursors.begin(); iter != frameCursors.end(); iter++) {
						TuioCursor *tcur = (*iter);
						TuioCursor *updateCursor = getTuioCursor(tcur->getSessionID());
						updateCursor->update(currentTime,tcur->getX(),tcur->getY(),tcur->getXSpeed(),tcur->getYSpeed(),tcur->getMotionAccel());
						
						for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++)
							(*listener)->updateTuioCursor(updateCursor);
							
						delete tcur;
					}
					
					for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++)
						(*listener)->refresh(currentTime);
					
					if (fseq>0) currentTime.reset();
				} else {
					for (std::list<TuioCursor*>::iterator iter=frameCursors.begin(); iter != frameCursors.end(); iter++) {
						TuioCursor *tcur = (*iter);
						delete tcur;
					}
				}
				
				frameCursors.clear();
			}
		}
	} catch( Exception& e ){
		std::cerr << "error parsing TUIO message: "<< msg.AddressPattern() <<  " - " << e.what() << std::endl;
	}
}

void TuioClient::ProcessPacket( const char *data, int size, const IpEndpointName& remoteEndpoint ) {
	ReceivedPacket p( data, size );
	if(p.IsBundle()) ProcessBundle( ReceivedBundle(p), remoteEndpoint);
	else ProcessMessage( ReceivedMessage(p), remoteEndpoint);
}

void TuioClient::connect(bool lk) {

#ifndef WIN32	
	/*pthread_mutexattr_settype(&attr_p, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&cursorMutex,&attr_p);
	pthread_mutex_init(&objectMutex,&attr_p);*/
	pthread_mutex_init(&cursorMutex,NULL);
	pthread_mutex_init(&objectMutex,NULL);	
#else
	cursorMutex = CreateMutex(NULL,FALSE,"cursorMutex");
	objectMutex = CreateMutex(NULL,FALSE,"objectMutex");
#endif		
		
	if (socket==NULL) return;
	TuioTime::initSession();
	currentTime.reset();
	
	locked = lk;
	if (!locked) {
#ifndef WIN32
		pthread_create(&thread , NULL, ClientThreadFunc, this);
#else
		DWORD threadId;
		thread = CreateThread( 0, 0, ClientThreadFunc, this, 0, &threadId );
#endif
	} else socket->Run();
	
	connected = true;
	unlockCursorList();
	unlockObjectList();
}

void TuioClient::disconnect() {
	
	if (socket==NULL) return;
	socket->Break();
	
	if (!locked) {
#ifdef WIN32
		if( thread ) CloseHandle( thread );
#endif
		thread = 0;
		locked = false;
	}
	
#ifndef WIN32	
	pthread_mutex_destroy(&cursorMutex);
	pthread_mutex_destroy(&objectMutex);
#else
	CloseHandle(cursorMutex);
	CloseHandle(objectMutex);
#endif
	
	connected = false;
}

void TuioClient::addTuioListener(TuioListener *listener) {
	listenerList.push_back(listener);
}

void TuioClient::removeTuioListener(TuioListener *listener) {
	std::list<TuioListener*>::iterator result = find(listenerList.begin(),listenerList.end(),listener);
	if (result!=listenerList.end()) listenerList.remove(listener);
}

TuioObject* TuioClient::getTuioObject(long s_id) {
	lockObjectList();
	for (std::list<TuioObject*>::iterator iter=objectList.begin(); iter != objectList.end(); iter++) {
		if((*iter)->getSessionID()==s_id) {
			unlockObjectList();
			return (*iter);
		}
	}	
	unlockObjectList();
	return NULL;
}

TuioCursor* TuioClient::getTuioCursor(long s_id) {
	lockCursorList();
	for (std::list<TuioCursor*>::iterator iter=cursorList.begin(); iter != cursorList.end(); iter++) {
		if((*iter)->getSessionID()==s_id) {
			unlockCursorList();
			return (*iter);
		}
	}	
	unlockCursorList();
	return NULL;
}

std::list<TuioObject*> TuioClient::getTuioObjects() {
	lockObjectList();
	std::list<TuioObject*> listBuffer = objectList;
	unlockObjectList();
	return listBuffer;
}

std::list<TuioCursor*> TuioClient::getTuioCursors() {
	lockCursorList();
	std::list<TuioCursor*> listBuffer = cursorList;
	unlockCursorList();
	return listBuffer;
}
