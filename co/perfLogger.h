
/* Copyright (c) 2014, David Steiner <steiner@ifi.uzh.ch>
 *
 * This file is part of Collage <https://github.com/Eyescale/Collage>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef CO_PERFLOGGER_H
#define CO_PERFLOGGER_H

#include <co/types.h>

namespace co
{

class PerfLogger
{
public:
    /** Log message. */
    virtual void log( const std::string& message ) = 0;

    /** Log message. */
    virtual void log( const uint128_t& id, const std::string& name, const std::string& message, const std::string& src ) = 0;

    /** Log message. */
    virtual void log(int64_t time, const co::NodeID& nodeID, const std::string& name, const std::string& message, const std::string& src ) = 0;
    
    virtual ~PerfLogger() {}
};

}
#endif  // CO_PERFLOGGER_H
