/*
 * server_pkt.h - Declarations for server_pkt.cpp
 * $Id: server_pkt.h,v 1.2 2004/06/14 21:52:32 rdenisc Exp $
 *
 * See "Teredo: Tunneling IPv6 over UDP through NATs"
 * for more information
 */

/***********************************************************************
 *  Copyright (C) 2004 Remi Denis-Courmont.                            *
 *  This program is free software; you can redistribute and/or modify  *
 *  it under the terms of the GNU General Public License as published  *
 *  by the Free Software Foundation; version 2 of the license.         *
 *                                                                     *
 *  This program is distributed in the hope that it will be useful,    *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of     *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.               *
 *  See the GNU General Public License for more details.               *
 *                                                                     *
 *  You should have received a copy of the GNU General Public License  *
 *  along with this program; if not, you can get it from:              *
 *  http://www.gnu.org/copyleft/gpl.html                               *
 ***********************************************************************/

#ifndef MIREDO_SERVER_PKT_H
# define MIREDO_SERVER_PKT_H

class MiredoServerUDP;

/*
 * Checks and handles an Teredo-encapsulated packet.
 */
int
handle_server_packet (const MiredoServerUDP *sock);

#endif /* ifndef MIREDO_SERVER_PKT_H */

