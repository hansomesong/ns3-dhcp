/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 UPB
 * Copyright (c) 2017 NITK Surathkal
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Radu Lupu <rlupu@elcom.pub.ro>
 *         Ankit Deepak <adadeepak8@gmail.com>
 *         Deepti Rajagopal <deeptir96@gmail.com>
 *
 */

#ifndef DHCP_SERVER_H
#define DHCP_SERVER_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/address.h"
#include <ns3/traced-value.h>
#include "dhcp-header.h"
#include <map>

namespace ns3 {

class Socket;
class Packet;

/**
 * \ingroup dhcp
 *
 * \class DhcpServer
 * \brief Implements the functionality of a DHCP server
 */
class DhcpServer : public Application
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Constructor
   */
  DhcpServer ();

  /**
   * \brief Destructor
   */
  virtual ~DhcpServer ();

  static const uint16_t PORT = 67;    //!< Port number of DHCP server

protected:
  virtual void DoDispose (void);

private:

  /*
   * \brief Handles incoming packets from the network
   * \param socket Socket bound to port 67 of the DHCP server
   */
  void NetHandler (Ptr<Socket> socket);

  /*
   * \brief Sends DHCP offer after receiving DHCP Discover
   * \param header DHCP header of the received message
   * \param from Address of the DHCP client
   */
  void SendOffer (DhcpHeader header, Address from);

  /*
   * \brief Sends DHCP ACK (or NACK) after receiving Request
   * \param header DHCP header of the received message
   * \param from Address of the DHCP client
   */
  void SendAck (DhcpHeader header, Address from);

  /*
   * \brief Modifies the remaining lease time of addresses
   */
  void TimerHandler (void);

  /*
   * \brief Starts the DHCP Server application
   */
  virtual void StartApplication (void);

  /*
   * \brief Stops the DHCP client application
   */
  virtual void StopApplication (void);

  Ptr<Socket> m_socket;                  //!< The socket bound to port 67
  Address m_local;                       //!< The local address
  Ipv4Address m_poolAddress;             //!< The network address available to the server
  Ipv4Address m_minAddress;              //!< The first address in the address pool
  Ipv4Address m_maxAddress;              //!< The last address in the address pool
  uint32_t m_occupiedRange;              //!< Number of occupied address in the pool
  Ipv4Mask m_poolMask;                   //!< The network mask of the pool
  Ipv4Address m_server;                  //!< Address of DHCP server
  Ipv4Address m_peer;                    //!< Address of DHCP client
  Ipv4Address m_gateway;                 //!< Store the gateway address
  std::map<std::pair<Address, Ipv4Address>, uint32_t> m_leasedAddresses; //!< Leased address and their status (cache memory)
  uint32_t m_nextAddressSeq;             //!< The next address in the sequence which can be alloted
  Time m_lease;                          //!< The granted lease time for an address
  Time m_renew;                          //!< The renewal time for an address
  Time m_rebind;                         //!< The rebinding time for an address
  EventId m_expiredEvent;                //!< The Event to trigger TimerHandler

  /**
   * To support LISP-MN, apart from m_socket communicating with DHCP client,
   * we need to add a m_lispMappingSocket (declared as Socket but created as
   * LispMappingSocket) to communicate with LispOverIpv4 object in the same node.
   * (Note that we need to check whether LispOverIpv4 object is present in the node,
   * if yes, create m_lispMappingSocket and connect it to m_lispProtoAddress and
   * send newly assigned RLOC to LispOverIpv4 to insert the former to lisp database.)
   */


};

} // namespace ns3

#endif /* DHCP_SERVER_H */
