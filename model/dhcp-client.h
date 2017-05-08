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

#ifndef DHCP_CLIENT_H
#define DHCP_CLIENT_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/socket.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-value.h"
#include "dhcp-header.h"
#include "ns3/endpoint-id.h"
#include <list>


namespace ns3 {

class Packet;
class Ipv4RawSocketImpl;

/**
 * \ingroup dhcp
 *
 * \class DhcpClient
 * \brief Implements the functionality of a DHCP client
 */
class DhcpClient : public Application
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId
  GetTypeId (void);

  /**
   * \brief Constructor
   */
  DhcpClient ();

  /**
   * \brief Destructor
   */
  virtual ~DhcpClient ();

  /**
   * \brief Get the IPv4Address of current DHCP server
   * \return Ipv4Address of current DHCP server
   */
  Ipv4Address GetDhcpServer (void);

  /**
   * Assign a fixed random variable stream number to the random variables
   * used by this model. Return the number of streams (possibly zero) that
   * have been assigned.
   *
   * \param stream First stream index to use
   * \return the number of stream indices assigned by this model
   */
  int64_t AssignStreams (int64_t stream);

protected:
  virtual void DoDispose (void);

private:
  enum States
  {
    WAIT_OFFER = 1,             //!< State of a client that waits for the offer
    REFRESH_LEASE = 2,          //!< State of a client that needs to refresh the lease
    WAIT_ACK = 9                //!< State of a client that waits for acknowledgment
  };

  static const uint16_t PORT = 68; //!< DHCP client port

  /*
   * \brief Starts the DHCP client application
   */
  virtual void StartApplication (void);

  void InitializeAddress(void);

  /*
   * \brief check whether the received gateway in DHCP offer is alread in static routing table
   */
  bool isGateWayExist (Ipv4Address m_gateway);

  /*
   * \brief as soon as default gateway is received from DHCP, delete all 0 static routes...
   */
  void RemoveAllZerosStaticRoute();

  /*
   * \brief Stops the DHCP client application
   */
  virtual void StopApplication (void);

  /*
   * \brief Handles changes in LinkState
   */
  void LinkStateHandler (void);

  /*
   * \brief Handles incoming packets from the network
   * \param socket Socket bound to port 68 of the DHCP client
   */
  void NetHandler (Ptr<Socket> socket);

  /*
   * \brief Sends DHCP DISCOVER and changes the client state to WAIT_OFFER
   */
  void Boot (void);

  /*
   * \brief Stores DHCP offers in m_offerList
   * \param header DhcpHeader of the DHCP OFFER message
   */
  void OfferHandler (DhcpHeader header);

  /*
   * \brief Selects an OFFER from m_offerList
   */
  void Select (void);

  /*
   * \brief Sends the DHCP REQUEST message and changes the client state to WAIT_ACK
   */
  void Request (void);

  /*
   * \brief Receives the DHCP ACK and configures IP address of the client.
   *        It also triggers the timeout, renew and rebind events.
   * \param header DhcpHeader of the DHCP ACK message
   * \param from   Address of DHCP server that sent the DHCP ACK
   */
  void AcceptAck (DhcpHeader header, Address from);

  /*
   * \brief Check if the current node is LISP-MN (i.e. has a LispOverIpv4 object)
   * \return true if LispOverIpv4 object is present in the node
   */
  bool IsLispCompatible();

  /*
   * \brief Check if the current node is LISP-MN and map table is created
   *  (i.e. has a LispOverIpv4 object which has a non-zero m_mapTables attribute)
   * \return true
   */
  bool IsLispDataBasePresent();

  /*
   * \brief Get TUN device interface index
   * \return interface index
   */
  uint32_t GetIfTunIndex();

  /*
   * \brief Receives the DHCP ACK and configures IP address of the client.
   *        It also triggers the timeout, renew and rebind events.
   * \param header DhcpHeader of the DHCP ACK message
   * \param from   Address of DHCP server that sent the DHCP ACK
   * \return tunDeviceIndex, if 0 => no TUN device otherwise the index of TUN device
   */
  Ptr<EndpointId> GetEid();

  /*
   * \brief After receiving Ack. If LISP database is not created
   * (i.e. when LISP-MN starts, it has no RLOC (i.e. 0.0.0.0/0 as IP address)),
   * It should create a Map Table for LispOverIpv4. If Lisp database is there,
   * Two possible scenario:
   * 1) LISP still in the same network, newly assigned RLOC is still in the same network
   * than the previous one.
   * 2) LISP roams into a different network. During roaming, the previous wifi link is lost then
   * new wifi link is established. what actions will be trigger (Link state change => delete IP address
   * => delete data base, but keep cache)?
   *
   * if database exists:
   * 	update EID-RLOC mapping.
   * 	if RLOC is the same with the previous one
   * 		do nothing.
   * 	else
   * 		Send Map-Register message for the new EID-RLOC mapping
   * else if database does not exist:
   * 	create database
   * 	add EID-RLOC mapping.
   * 	Send Map-Register message for new EID-RLOC mapping
   */
  void LispDataBaseManipulation(Ptr<EndpointId> eid);

  /*
   * \brief Remove the current DHCP information and restart the process
   */
  void RemoveAndStart ();

  /*
   * \brief Create the Ipv4RawSocket
   */
  void CreateSocket ();

  /*
   * \brief Close the Ipv4RawSocket
   */
  void CloseSocket ();

  uint8_t m_state;                       //!< State of the DHCP client
  uint32_t m_device;                     //!< Device identifier
  Ptr<Ipv4RawSocketImpl> m_socket;       //!< Socket for remote communication
  Ipv4Address m_remoteAddress;           //!< Initially set to 255.255.255.255 to start DHCP
  Ipv4Address m_offeredAddress;          //!< Address offered to the client
  Ipv4Address m_myAddress;               //!< Address assigned to the client
  Ipv4Mask m_myMask;                     //!< Mask of the address assigned
  Ipv4Address m_server;                  //!< Address of the DHCP server
  Ipv4Address m_gateway;                 //!< Address of the gateway
  EventId m_requestEvent;                //!< Address refresh event
  EventId m_discoverEvent;               //!< Message retransmission event
  EventId m_refreshEvent;                //!< Message refresh event
  EventId m_rebindEvent;                 //!< Message rebind event
  EventId m_nextOfferEvent;              //!< Message next offer event
  EventId m_timeout;                     //!< The timeout period
  Time m_lease;                          //!< Store the lease time of address
  Time m_renew;                          //!< Store the renew time of address
  Time m_rebind;                         //!< Store the rebind time of address
  Time m_nextoffer;                      //!< Time to try the next offer (if request gets no reply)
  Ptr<RandomVariableStream> m_ran;       //!< Uniform random variable for transaction ID
  Time m_rtrs;                           //!< Defining the time for retransmission
  Time m_collect;                        //!< Time for which client should collect offers
  bool m_offered;                        //!< Specify if the client has got any offer
  std::list<DhcpHeader> m_offerList;     //!< Stores all the offers given to the client
  uint32_t m_tran;                       //!< Stores the current transaction number to be used
  TracedCallback<const Ipv4Address&> m_newLease;//!< Trace of new lease
  TracedCallback<const Ipv4Address&> m_expiry;  //!< Trace of expiry

  //!<a flag to indicate whether to trigger LISP-related manipulation.
  //for example, if DHCP cleint obtains a different IP@ than the previous one.
  //do lisp-related manipulation otherwise no.
  bool m_trigLisp;

  /**
   * To support LISP-MN, apart from m_socket communicating with DHCP client,
   * we need to add a m_lispMappingSocket (declared as Socket but created as
   * LispMappingSocket) to communicate with LispOverIpv4 object in the same node.
   * (Note that we need to check whether LispOverIpv4 object is present in the node,
   * if yes, create m_lispMappingSocket and connect it to m_lispProtoAddress and
   * send newly assigned RLOC to LispOverIpv4 to insert the former to lisp database.)
   */

  Address m_lispProtoAddress;
  Ptr<Socket> m_lispMappingSocket;

  void HandleMapSockRead (Ptr<Socket> lispMappingSocket);


};

} // namespace ns3

#endif /* DHCP_CLIENT_H */
