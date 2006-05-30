
/* Copyright (c) 2005, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#ifndef EQBASE_REQUESTHANDLER_H
#define EQBASE_REQUESTHANDLER_H

#include "base.h"
#include "hash.h"
#include "referenced.h"
#include "thread.h"
#include "timedLock.h"

#include <list>

namespace eqBase
{
    class Lock;

    /**
     * A request handler.
     * 
     * Different execution threads can synchronize using a request handler. One
     * thread registers a request. Another thread can serve the request. The
     * original thread can wait for the request to be served and retrieve the
     * result.
     *
     * A note on thread-safety: Unless threadSafe is set in the contructor, the
     * functions registerRequest(), unregisterRequest() and waitRequest() are
     * supposed to be called from one 'waiting' thread, and the functions
     * serveRequest() and deleteRequest() are supposed to be called only from
     * one 'serving' thread.
     */
    class RequestHandler : public Referenced
    {

    public:
        /** 
         * Constructs a new requestHandler of the given type.
         * 
         * @param type the type of threads accessing the requestHandler.
         * @param threadSafe if <code>true</code>, all public functions are
         *                   thread-safe.
         */
        RequestHandler( const Thread::Type type = Thread::PTHREAD,
                        const bool threadSafe = false );

        /** Destructs the requestHandler. */
        ~RequestHandler();

        /** 
         * Registers a new request.
         * 
         * @param data a pointer to user-specific data for the request, can be
         *             <code>NULL</code>.
         * @return the request identifier.
         */
        uint32_t registerRequest( void* data = NULL );

        /** 
         * Unregisters a request.
         *
         * Note that waitRequest() automatically unregisters the request when it
         * was successful.
         * 
         * @param requestID the request identifier.
         */
        void unregisterRequest( const uint32_t requestID );

        /** 
         * Waits a given time for the completion of a request.
         *
         * If the request was served, it is unregistered and the request result
         * is returned.
         * 
         * @param requestID the request identifier.
         * @param success return value to indicate if the request was served.
         * @param timeout the timeout in milliseconds to wait for the request,
         *                or <code>EQ_TIMEOUT_INDEFINITE</code> to wait
         *                indefinitely.
         * @return the result of the request, or <code>NULL</code> if the
         *         request was not served.
         */
        void* waitRequest( const uint32_t requestID, bool* success = NULL,
                           const uint32_t timeout = EQ_TIMEOUT_INDEFINITE );

        /** 
         * Retrieves the user-specific data for a request.
         * 
         * @param requestID the request identifier.
         * @return the user-specific data for the request.
         */
        void* getRequestData( const uint32_t requestID );

        /** 
         * Server a request.
         * 
         * @param requestID the request identifier.
         * @param result the result of the request.
         */
        void serveRequest( const uint32_t requestID, void* result );

        /** 
         * Deletes a request.
         * 
         * The caller has to ensure that nobody is or will wait on the request.
         *
         * @param requestID the request identifier.
         */
        void deleteRequest( const uint32_t requestID );

    private:
        Thread::Type _type;
        Lock*        _mutex;

        struct Request
        {
            Request( Thread::Type type )
                {
                    lock = new TimedLock( type );
                    lock->set();
                }
            ~Request(){ delete lock; }
            
            TimedLock* lock;
            void*      data;
            void*      result;
        };
        
        typedef Sgi::hash_map<uint32_t, Request*> RequestHash;

        uint32_t            _requestID;
        RequestHash         _requests;
        std::list<Request*> _freeRequests;

        std::vector<uint32_t> _deletedRequests;
        Lock*                 _deletedRequestsLock;

        void _recycleRequests();
        void _unregisterRequest( const uint32_t requestID );

#ifdef CHECK_THREADSAFETY
        pthread_t _threadID;
#endif
    };
}

#endif //EQBASE_REQUESTHANDLER_H
