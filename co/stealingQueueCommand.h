
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

#ifndef CO_STEALINGQUEUECOMMAND_H
#define CO_STEALINGQUEUECOMMAND_H

#include <co/queueCommand.h>

namespace co
{
    enum StealingQueueCommand
    {
        CMD_QUEUE_STEAL_ITEM = CMD_QUEUE_CUSTOM,
        CMD_MASTER_QUEUE_EMPTY,
        CMD_QUEUE_ITEM_LEFT,
        CMD_QUEUE_ITEM_RIGHT,
        CMD_QUEUE_DENY,
        CMD_QUEUE_DENY_MASTER,
        CMD_QUEUE_STOLEN_ITEM,
        CMD_QUEUE_VICTIM_DATA,
        CMD_QUEUE_SLAVE_FEEDBACK,
        CMD_QUEUE_HELLO
    };
}

#endif // CO_STEALINGQUEUECOMMAND_H
