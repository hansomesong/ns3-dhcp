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
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <list>

#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/random-variable-stream.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/pointer.h"
#include "ns3/string.h"
#include "ns3/ipv4-raw-socket-impl.h"
#include "ns3/ipv4-raw-socket-factory.h"
#include "ns3/udp-l4-protocol.h"
#include "ns3/udp-header.h"
#include "ns3/lisp-mapping-socket.h" //To support LISP with DHCP!
#include "ns3/mapping-socket-msg.h" //To support LISP with DHCP!
#include "ns3/lisp-over-ipv4.h"
//#include "ns3/locators-impl.h"
#include "ns3/simple-map-tables.h"


#include "dhcp-client.h"
#include "dhcp-server.h"
#include "dhcp-header.h"

namespace ns3
{

  NS_LOG_COMPONENT_DEFINE("DhcpClient");
  NS_OBJECT_ENSURE_REGISTERED(DhcpClient);

  TypeId
  DhcpClient::GetTypeId (void)
  {
    static TypeId tid =
	TypeId ("ns3::DhcpClient").SetParent<Application> ().AddConstructor<
	    DhcpClient> ().SetGroupName ("Internet-Apps").AddAttribute (
	    "NetDevice", "Index of netdevice of the node for DHCP",
	    UintegerValue (0), MakeUintegerAccessor (&DhcpClient::m_device),
	    MakeUintegerChecker<uint32_t> ()).AddAttribute (
	    "RTRS", "Time for retransmission of Discover message",
	    TimeValue (Seconds (5)), MakeTimeAccessor (&DhcpClient::m_rtrs),
	    MakeTimeChecker ()).AddAttribute (
	    "Collect", "Time for which offer collection starts",
	    TimeValue (Seconds (1.0)),
	    MakeTimeAccessor (&DhcpClient::m_collect), MakeTimeChecker ()).AddAttribute (
	    "ReRequest",
	    "Time after which request will be resent to next server",
	    TimeValue (Seconds (10)),
	    MakeTimeAccessor (&DhcpClient::m_nextoffer), MakeTimeChecker ()).AddAttribute (
	    "Transactions", "The possible value of transaction numbers ",
	    StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1000000.0]"),
	    MakePointerAccessor (&DhcpClient::m_ran),
	    MakePointerChecker<RandomVariableStream> ()).AddTraceSource (
	    "NewLease", "Get a NewLease",
	    MakeTraceSourceAccessor (&DhcpClient::m_newLease),
	    "ns3::Ipv4Address::TracedCallback").AddTraceSource (
	    "Expiry", "A lease expires",
	    MakeTraceSourceAccessor (&DhcpClient::m_expiry),
	    "ns3::Ipv4Address::TracedCallback");
    return tid;
  }

  DhcpClient::DhcpClient () :
      m_server (Ipv4Address::GetAny ())
  {
    NS_LOG_FUNCTION_NOARGS ();
    m_socket = 0;
    m_lispMappingSocket =0;
    m_refreshEvent = EventId ();
    m_requestEvent = EventId ();
    m_discoverEvent = EventId ();
    m_rebindEvent = EventId ();
    m_nextOfferEvent = EventId ();
    m_timeout = EventId ();
    m_trigLisp = false;
    m_remoteAddress = Ipv4Address ("255.255.255.255");
    m_myAddress = Ipv4Address ("0.0.0.0");
    m_gateway = Ipv4Address ("0.0.0.0");
  }

  DhcpClient::~DhcpClient ()
  {
    NS_LOG_FUNCTION_NOARGS ();
  }

  Ipv4Address
  DhcpClient::GetDhcpServer (void)
  {
    return m_server;
  }

  void
  DhcpClient::DoDispose (void)
  {
    NS_LOG_FUNCTION_NOARGS ();
    Application::DoDispose ();
  }

  int64_t
  DhcpClient::AssignStreams (int64_t stream)
  {
    NS_LOG_FUNCTION(this << stream);
    m_ran->SetStream (stream);
    return 1;
  }

  void
  DhcpClient::CreateSocket ()
  {
    NS_LOG_FUNCTION(this);
    NS_LOG_DEBUG("DHCP client tries to create Ipv4RawSocket...");
    if (m_socket != 0)
      {
	NS_LOG_WARN("DHCP client has already a Ipv4RawSocket!");
      }
    if (m_socket == 0)
      {
	TypeId tid = TypeId::LookupByName ("ns3::Ipv4RawSocketFactory");
	m_socket = DynamicCast<Ipv4RawSocketImpl> (
	    Socket::CreateSocket (GetNode (), tid));
	m_socket->SetProtocol (UdpL4Protocol::PROT_NUMBER);
	m_socket->SetAllowBroadcast (true);
	m_socket->Bind ();
	m_socket->BindToNetDevice (GetNode ()->GetDevice (m_device));
	m_socket->SetRecvCallback (MakeCallback (&DhcpClient::NetHandler, this));
      }

    Ptr<LispOverIp> lisp = GetNode ()->GetObject<LispOverIp> ();
    if (m_lispMappingSocket !=0)
      {
	NS_LOG_WARN("DHCP client has already a Lisp Mapping Socket!");
      }
    else if (lisp != 0 and m_lispMappingSocket ==0)
      {
	NS_LOG_DEBUG(
	    "DHCP client is trying to connect to data plan (actually LispOverIpv4 object).");
	// #2: create lispMappingSocket object, which is declared as Socket
	TypeId tid = TypeId::LookupByName ("ns3::LispMappingSocketFactory");
	m_lispMappingSocket = Socket::CreateSocket (GetNode (), tid);
	Ptr<LispOverIp> lisp = m_lispMappingSocket->GetNode ()->GetObject<
	    LispOverIp> ();
	m_lispProtoAddress = lisp->GetLispMapSockAddress ();
	m_lispMappingSocket->Bind ();
	m_lispMappingSocket->Connect (m_lispProtoAddress);
	//It is very possible that m_lispProtoAddress is a special kind of address
	//which is not an Ipv4 neither Ipv6 address.
	NS_LOG_DEBUG("DHCP client has connected to " << m_lispProtoAddress);
	m_lispMappingSocket->SetRecvCallback (
	    MakeCallback (&DhcpClient::HandleMapSockRead, this));
      }
    NS_LOG_DEBUG("DHCP client finishes the socket create process");
  }

  void DhcpClient::HandleMapSockRead (Ptr<Socket> lispMappingSocket)
  {
    NS_LOG_FUNCTION (this);
    Ptr<Packet> packet;
    Address from;
    while ((packet = lispMappingSocket->RecvFrom (from)))
    {
  	  NS_LOG_DEBUG("Receive sth from lisp. Now noting to do...");
    }
  }

  void
  DhcpClient::CloseSocket ()
  {
    m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    m_socket->Close ();
    Ptr<LispOverIp> lisp = GetNode ()->GetObject<LispOverIp> ();
    if (lisp != 0)
      {
	m_lispMappingSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
	m_lispMappingSocket->Close ();
      }
  }

  void
  DhcpClient::StartApplication (void)
  {
    NS_LOG_FUNCTION_NOARGS ();
    NS_LOG_DEBUG("At the start of DCHP client application, Wifi Net device's status: "<<GetNode ()->GetDevice (m_device)->IsLinkUp ());
    InitializeAddress();
    CreateSocket ();
    Boot ();
    /**
     * Append a Callback to the chain. I wonder what the chain refers to.
     * It seems a container which can hold more than one elements.
     * It's also logical for a net device to trigger more several callbacks if one event
     * triggers. Thus, I think, here we should not add a Link change callback function
     * each time DHCP client is started. Because link state change handler will start DHCP
     * client which add link chagne callback function. Hence, I think, it is better to
     * add this hanlder just in DHCP's constructor.
     */
    GetNode ()->GetDevice (m_device)->AddLinkChangeCallback (
	MakeCallback (&DhcpClient::LinkStateHandler, this));
    NS_LOG_DEBUG("Add a Link Change Callback to net device with index: "<<unsigned(m_device));
  }

  void DhcpClient::InitializeAddress()
  {
    Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();
    uint32_t ifIndex = ipv4->GetInterfaceForDevice (
	GetNode ()->GetDevice (m_device));
//    NS_LOG_DEBUG("DHCP client is running on interface: "<<unsigned(ifIndex));
    bool found = false;
    for (uint32_t i = 0; i < ipv4->GetNAddresses (ifIndex); i++)
      {
	if (ipv4->GetAddress (ifIndex, i).GetLocal () == m_myAddress)
	  {
	    found = true;
	  }
      }
    if (!found)
      {
	ipv4->AddAddress (
	    ifIndex,
	    Ipv4InterfaceAddress (Ipv4Address ("0.0.0.0"), Ipv4Mask ("/0")));
      }
  }

  void
  DhcpClient::StopApplication ()
  {
    NS_LOG_FUNCTION_NOARGS ();
    Simulator::Remove (m_discoverEvent);
    Simulator::Remove (m_requestEvent);
    Simulator::Remove (m_rebindEvent);
    Simulator::Remove (m_refreshEvent);
    Simulator::Remove (m_timeout);
    Simulator::Remove (m_nextOfferEvent);
    Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();

    int32_t ifIndex = ipv4->GetInterfaceForDevice (
	GetNode ()->GetDevice (m_device));
    NS_ASSERT(ifIndex >= 0);
    for (uint32_t i = 0; i < ipv4->GetNAddresses (ifIndex); i++)
      {
	if (ipv4->GetAddress (ifIndex, i).GetLocal () == m_myAddress)
	  {
	    ipv4->RemoveAddress (ifIndex, i);
	    break;
	  }
      }

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (
	ipv4);
    uint32_t i;
    for (i = 0; i < staticRouting->GetNRoutes (); i++)
      {
	if (staticRouting->GetRoute (i).GetGateway () == m_gateway
	    && staticRouting->GetRoute (i).GetInterface () == (uint32_t) ifIndex
	    && staticRouting->GetRoute (i).GetDest ()
		== Ipv4Address ("0.0.0.0"))
	  {
	    staticRouting->RemoveRoute (i);
	    break;
	  }
      }
    CloseSocket ();
  }

  void
  DhcpClient::LinkStateHandler (void)
  {
    NS_LOG_FUNCTION_NOARGS ();
    bool linkUp = GetNode ()->GetDevice (m_device)->IsLinkUp ();
    if (linkUp)
      {
	/**
	 * ATTENTION: It seems that AdHoc has no link change... I never find this
	 * handler is triggered until now if using Adhoc mode.
	 */
	NS_LOG_INFO("Link up at " << Simulator::Now ().GetSeconds ());
	InitializeAddress();
	CreateSocket ();
	Boot ();
      }
    else if(not linkUp)
      {
	//TODO: If Link down, under LISP-MN => Assigned RLOC is lost => Need to update lisp
	//data plan database.
//	NS_ASSERT_MSG(true==false, "link state chagne, we catch u! Now can delete this assert...");
	NS_LOG_INFO("Link down at " << Simulator::Now ().GetSeconds ()); //reinitialization
	Simulator::Remove (m_refreshEvent); //stop refresh timer!!!!
	Simulator::Remove (m_rebindEvent);
	Simulator::Remove (m_timeout);
	m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ()); //stop receiving on this socket !!!

	Ptr<Ipv4> ipv4MN = GetNode ()->GetObject<Ipv4> ();
	int32_t ifIndex = ipv4MN->GetInterfaceForDevice (
	    GetNode ()->GetDevice (m_device));
	NS_ASSERT_MSG(ifIndex >= 0, "interface index should be >=0, but:"<<ifIndex);
	for (uint32_t i = 0; i < ipv4MN->GetNAddresses (ifIndex); i++)
	  {
	    if (ipv4MN->GetAddress (ifIndex, i).GetLocal () == m_myAddress)
	      {
		ipv4MN->RemoveAddress (ifIndex, i);
		// Still set ipv4 address to 0.0.0.0
		// Since when link down, it is still possible to transmit IP packet,
		// which causes problems if no IP address.
		ipv4MN->AddAddress (
		    ifIndex,
		    Ipv4InterfaceAddress (Ipv4Address ("0.0.0.0"), Ipv4Mask ("/0")));
		break;
	      }
	  }

	Ipv4StaticRoutingHelper ipv4RoutingHelper;
	Ptr<Ipv4StaticRouting> staticRouting =
	    ipv4RoutingHelper.GetStaticRouting (ipv4MN);
	uint32_t i;
	for (i = 0; i < staticRouting->GetNRoutes (); i++)
	  {
	    if (staticRouting->GetRoute (i).GetGateway () == m_gateway
		&& staticRouting->GetRoute (i).GetInterface ()
		    == (uint32_t) ifIndex
		&& staticRouting->GetRoute (i).GetDest ()
		    == Ipv4Address ("0.0.0.0"))
	      {
		staticRouting->RemoveRoute (i);
		break;
	      }
	  }
	NS_LOG_INFO("Finish to processing link down related manipulation...");
      }
  }

  void
  DhcpClient::NetHandler (Ptr<Socket> socket)
  {
    NS_LOG_FUNCTION(this << socket);
    Address from;
    Ptr<Packet> packet = m_socket->RecvFrom (2048, 0, from);

    Ipv4Header ipHeader;
    UdpHeader udpHeader;
    packet->RemoveHeader (ipHeader);
    packet->RemoveHeader (udpHeader);

    if (udpHeader.GetDestinationPort () != PORT)
      {
	return;
      }

    DhcpHeader header;
    if (packet->RemoveHeader (header) == 0)
      {
	return;
      }
    if (header.GetChaddr () != GetNode ()->GetDevice (m_device)->GetAddress ())
      {
	return;
      }
    if (m_state == WAIT_OFFER && header.GetType () == DhcpHeader::DHCPOFFER)
      {
	OfferHandler (header);
      }
    if (m_state == WAIT_ACK && header.GetType () == DhcpHeader::DHCPACK)
      {
	Simulator::Remove (m_nextOfferEvent);
	AcceptAck (header, from);
      }
    if (m_state == WAIT_ACK && header.GetType () == DhcpHeader::DHCPNACK)
      {
	Simulator::Remove (m_nextOfferEvent);
	Boot ();
      }
  }

  void
  DhcpClient::Boot (void)
  {
    NS_LOG_FUNCTION_NOARGS ();
    DhcpHeader dhcpHeader;
    UdpHeader udpHeader;
    Ptr<Packet> packet;
    packet = Create<Packet> ();

    dhcpHeader.ResetOpt ();
    m_tran = (uint32_t) (m_ran->GetValue ());
    dhcpHeader.SetTran (m_tran);
    dhcpHeader.SetType (DhcpHeader::DHCPDISCOVER);
    dhcpHeader.SetTime ();
    dhcpHeader.SetChaddr (GetNode ()->GetDevice (m_device)->GetAddress ());
    packet->AddHeader (dhcpHeader);

    udpHeader.SetDestinationPort (DhcpServer::PORT);
    udpHeader.SetSourcePort (PORT);
    if (Node::ChecksumEnabled ())
      {
	udpHeader.EnableChecksums ();
	udpHeader.InitializeChecksum (m_myAddress,
				      Ipv4Address ("255.255.255.255"),
				      UdpL4Protocol::PROT_NUMBER);
      }
    packet->AddHeader (udpHeader);

    if ((m_socket->SendTo (packet, 0,
			   InetSocketAddress (Ipv4Address ("255.255.255.255"))))
	>= 0)
      {
	NS_LOG_INFO(
	    "At time " << Simulator::Now ().GetSeconds () << "s DHCP DISCOVER sent. Content: "<<*packet);
      }
    else
      {
	NS_LOG_INFO("Error while sending DHCP DISCOVER to " << m_remoteAddress);
      }
    m_state = WAIT_OFFER;
    m_offered = false;
    m_discoverEvent = Simulator::Schedule (m_rtrs, &DhcpClient::Boot, this);
  }

  void
  DhcpClient::OfferHandler (DhcpHeader header)
  {
    m_offerList.push_back (header);
    if (m_offered == false)
      {
	Simulator::Remove (m_discoverEvent);
	m_offered = true;
	Simulator::Schedule (m_collect, &DhcpClient::Select, this);
      }
  }

  void
  DhcpClient::Select (void)
  {
    NS_LOG_FUNCTION_NOARGS ();
    if (m_offerList.empty ())
      {
	return;
      }
    DhcpHeader header = m_offerList.front ();
    m_offerList.pop_front ();
    m_lease = Time (Seconds (header.GetLease ()));
    m_renew = Time (Seconds (header.GetRenew ()));
    m_rebind = Time (Seconds (header.GetRebind ()));
    m_offeredAddress = header.GetYiaddr ();
    m_myMask = Ipv4Mask (header.GetMask ());
    m_server = header.GetDhcps ();
    m_gateway = header.GetRouter ();
    m_offerList.clear ();
    m_offered = false;
    Request ();
    NS_LOG_INFO(
	"At time " << Simulator::Now ().GetSeconds () << "s DHCP client sent DHCP REQUEST to DHCP Server");
  }

  void
  DhcpClient::Request (void)
  {
    NS_LOG_FUNCTION_NOARGS ();
    DhcpHeader dhcpHeader;
    UdpHeader udpHeader;
    Ptr<Packet> packet;
    if (m_state != REFRESH_LEASE)
      {
	packet = Create<Packet> ();

	dhcpHeader.ResetOpt ();
	dhcpHeader.SetType (DhcpHeader::DHCPREQ);
	dhcpHeader.SetTime ();
	dhcpHeader.SetTran (m_tran);
	dhcpHeader.SetReq (m_offeredAddress);
	dhcpHeader.SetChaddr (GetNode ()->GetDevice (m_device)->GetAddress ());
	packet->AddHeader (dhcpHeader);

	udpHeader.SetDestinationPort (DhcpServer::PORT);
	udpHeader.SetSourcePort (PORT);
	if (Node::ChecksumEnabled ())
	  {
	    udpHeader.EnableChecksums ();
	    udpHeader.InitializeChecksum (m_myAddress,
					  Ipv4Address ("255.255.255.255"),
					  UdpL4Protocol::PROT_NUMBER);
	  }
	packet->AddHeader (udpHeader);

	m_socket->SendTo (packet, 0,
			  InetSocketAddress (Ipv4Address ("255.255.255.255")));
	m_state = WAIT_ACK;
	m_nextOfferEvent = Simulator::Schedule (m_nextoffer,
						&DhcpClient::Select, this);
      }
    else
      {
	//CreateSocket ();
	uint32_t addr = m_myAddress.Get ();
	packet = Create<Packet> ((uint8_t*) &addr, sizeof(addr));

	dhcpHeader.ResetOpt ();
	m_tran = (uint32_t) (m_ran->GetValue ());
	dhcpHeader.SetTran (m_tran);
	dhcpHeader.SetTime ();
	dhcpHeader.SetType (DhcpHeader::DHCPREQ);
	dhcpHeader.SetReq (m_myAddress);
	m_offeredAddress = m_myAddress;
	dhcpHeader.SetChaddr (GetNode ()->GetDevice (m_device)->GetAddress ());
	packet->AddHeader (dhcpHeader);

	udpHeader.SetDestinationPort (DhcpServer::PORT);
	udpHeader.SetSourcePort (PORT);
	if (Node::ChecksumEnabled ())
	  {
	    udpHeader.EnableChecksums ();
	    udpHeader.InitializeChecksum (m_myAddress, m_remoteAddress,
					  UdpL4Protocol::PROT_NUMBER);
	  }
	packet->AddHeader (udpHeader);

	if ((m_socket->SendTo (packet, 0, InetSocketAddress (m_remoteAddress)))
	    >= 0)
	  {
	    NS_LOG_INFO("DHCP REQUEST sent");
	  }
	else
	  {
	    NS_LOG_INFO("Error while sending DHCP REQ to " << m_remoteAddress);
	  }
	m_state = WAIT_ACK;
      }
  }

  void
  DhcpClient::AcceptAck (DhcpHeader header, Address from)
  {
    /**
     * Note that attributes such as m_offeredAddress, m_gateway,etc. have been
     * configured in DhcpClient::Select.
     */
    Simulator::Remove (m_rebindEvent);
    Simulator::Remove (m_refreshEvent);
    Simulator::Remove (m_timeout);
    NS_LOG_INFO(
	"At time " << Simulator::Now ().GetSeconds () <<"s DHCP ACK received");
    Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();
    int32_t ifIndex = ipv4->GetInterfaceForDevice (
	GetNode ()->GetDevice (m_device));
    // Before assign the received m_offeredAddress, remove the previous @IP
    // However, one Ipv4Interface may have more than one Ipv4Address (e.g. IP Aliasing)
    // That's why we need to iterate each Ipv4Address on NetDevice where DHCP client manages.
    for (uint32_t i = 0; i < ipv4->GetNAddresses (ifIndex); i++)
      {
	if (ipv4->GetAddress (ifIndex, i).GetLocal () == m_myAddress)
	  {
	    ipv4->RemoveAddress (ifIndex, i);
	    break;
	  }
      }
    ipv4->AddAddress (
	ifIndex,
	Ipv4InterfaceAddress ((Ipv4Address) m_offeredAddress, m_myMask));
    ipv4->SetUp (ifIndex);

    if (m_myAddress != m_offeredAddress)
      {
	// A different IP@ to the previously assigned IP@, trigger LISP
	std::cout<<"A different @IP!"<<std::endl;
	m_trigLisp = true;
	m_newLease (m_offeredAddress);
	if (m_myAddress != Ipv4Address ("0.0.0.0"))
	  {
	    m_expiry (m_myAddress);
	  }
      }
    m_myAddress = m_offeredAddress;
    if (m_gateway != Ipv4Address ("0.0.0.0"))
      {
	Ipv4StaticRoutingHelper ipv4RoutingHelper;
	Ptr<Ipv4StaticRouting> staticRouting =
	    ipv4RoutingHelper.GetStaticRouting (ipv4);
	staticRouting->SetDefaultRoute (m_gateway, ifIndex, 0);
	//if LISP-MN compatible
	if(DhcpClient::IsLispCompatible())
	{
	    uint32_t ifTunIndex = DhcpClient::GetIfTunIndex();
	    if(ifTunIndex)
	    {
		staticRouting->AddNetworkRouteTo(Ipv4Address("0.0.0.0"), Ipv4Mask("/1"), ifTunIndex);
		staticRouting->AddNetworkRouteTo(Ipv4Address("128.0.0.0"), Ipv4Mask("/1"), ifTunIndex);
		//TODO: if wifi lost, whether to delete these two static routes?
		NS_LOG_DEBUG("As a LISP-MN, two static routes with /1 are added so that "
		    <<"application always use EID on TUN device as inner IP header source address");
	    }
	}
      }

    m_remoteAddress = InetSocketAddress::ConvertFrom (from).GetIpv4 ();
    NS_LOG_INFO("Current DHCP Server is " << m_remoteAddress);

    // Do lisp-related manipulation, such as create database, update map entry in database.
    // Just m_trigLisp = true, which means has a different @IP, need to send the newly
    // EID-RLOC mapping to lispOverIp.
    if(DhcpClient::IsLispCompatible() and m_trigLisp)
      {
	DhcpClient::LispDataBaseManipulation(DhcpClient::GetEid());
      }
    m_offerList.clear ();
    m_refreshEvent = Simulator::Schedule (m_renew, &DhcpClient::Request, this);
    m_rebindEvent = Simulator::Schedule (m_rebind, &DhcpClient::Request, this);
    m_timeout = Simulator::Schedule (m_lease, &DhcpClient::RemoveAndStart,
				     this);
    m_state = REFRESH_LEASE;
    // Do not forget to reset the following flag as false
    m_trigLisp = false;
  }

  bool DhcpClient::IsLispCompatible()
  {
    return GetNode()->GetObject<LispOverIpv4>() != 0;
  }

  bool DhcpClient::IsLispDataBasePresent()
  {
    bool result = false;
    if(DhcpClient::IsLispCompatible()){
	result = ((GetNode()->GetObject<LispOverIpv4>())->GetMapTablesV4()->GetNMapEntriesLispDataBase() != 0);
    }
    return result;
  }

  uint32_t DhcpClient::GetIfTunIndex()
  {
    NS_LOG_FUNCTION(this);
    Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();
    uint32_t ifTunIndex = 0;
    uint32_t tunDeviceIndex = 1;
    /**
     * Ignore the device with index 0 => Loopback device
     * Iterate Net device index and compare its type id name to find virtual net device index.
     * Then get @IP address
     */
    for (tunDeviceIndex = 1; tunDeviceIndex < GetNode ()->GetNDevices (); tunDeviceIndex++)
      {
	Ptr<NetDevice> curDev = GetNode ()->GetDevice (tunDeviceIndex);
	// Be careful with compare() method.
	if (curDev->GetInstanceTypeId ().GetName () == "ns3::VirtualNetDevice")
	  {
	    NS_LOG_DEBUG("TUN device index: "<<unsigned(tunDeviceIndex));
	    NS_LOG_DEBUG("Device type: "<<curDev->GetInstanceTypeId ().GetName ());
	    ifTunIndex = ipv4->GetInterfaceForDevice (
		GetNode ()->GetDevice (tunDeviceIndex));
	    NS_LOG_DEBUG("TUN interface Index: "<< unsigned(ifTunIndex));
	    /**
	     * An intutive difference between Ipv4InterfaceAddress and Ipv4Address is:
	     * the former container Ipv4Address along with Ipv4Maks, broacast, etc...
	     * The information we can see from "ifconfig eth0"
	     * TODO: Attention we assume that one LISP-MN just have one @IP!
	     * It is trival to extend to support several @IP on TUN, which is not necessary
	     * in this case!
	     */
	    //TODO: How to process the case where more than one TUN device on a node?
	    break;
	  }
      }
    //If return 0, means no TUN device
    return ifTunIndex;
  }

  Ptr<EndpointId> DhcpClient::GetEid()
  {
    NS_LOG_FUNCTION(this);
    Ptr<EndpointId> eid;
    uint32_t ifTunIndex = DhcpClient::GetIfTunIndex();
    Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();
    if(ifTunIndex)
    {
      Ipv4Address eidAddress =
	  ipv4->GetAddress (ifTunIndex, 0).GetLocal ();
//      Ipv4Mask eidMask = ipv4->GetAddress (ifTunIndex, 0).GetMask ();
      // LISP-MN, as a single machine, we use "/32" as EID mask
      eid = Create<EndpointId> (eidAddress, Ipv4Mask("/32"));
      NS_LOG_DEBUG("The retrieved EID:"<< eid->Print());
    }
    /**
     * TODO: Maybe we should consider the case where TUN net device is installed.
     * But, no @IP is assigned!
     */
    return eid;
  }

  void DhcpClient::LispDataBaseManipulation(Ptr<EndpointId> eid)
  {
    /**
     * Send Mapping socket control message to lispOverIpv4 object so that
     * lisp database can (if exist) add or update new EID-RLOC map entry
     *
     * TODO: Another solution: given that InsertLocators method is MapTable is public,
     * we can directly call related method to insert new EID-RLOC mapping into
     * database. The current applied solution is more modular: we send message to lispOverIpv4
     * and let the latter to manipulate the map tables.
     */
    //TODO:How if we use IP aliasing (secondary @IP on the Netdevice)?
    Ptr<LispOverIpv4> lisp = GetNode()->GetObject<LispOverIpv4>();
    Ptr<MappingSocketMsg> mapSockMsg = Create<MappingSocketMsg> ();
    Ptr<Locators> locators = Create<LocatorsImpl> ();
    /**
     * Construt message content: one EID and one unique RLOC (but we can support a list of RLOC)
     * 1) EID field in mapping socket message => Easy!
     * 2) RLOC list, default priority and weight make it as 100.
     * At last, remind that this message is not the lisp control plan message exchange between xTR
     * It is the message used between lisp data plan and control plan.
     * Here we use this between lisp data plan and DHCP
     */
    mapSockMsg->SetEndPoint (eid);
    Ptr<RlocMetrics> rlocMetrics = Create<RlocMetrics> (100, 100, true);
    rlocMetrics->SetMtu(1500);
    rlocMetrics->SetIsLocalIf(true);
    Ptr<Locator> locator = Create<Locator> (m_offeredAddress);
    locator->SetRlocMetrics (rlocMetrics);
    locators->InsertLocator (locator);
    mapSockMsg->SetLocators (locators);

    /**
     * Construct message header
     * Note:
     * 1) unlike Java, an instruction like: MappingSocketMsgHeader mapSockHeader;
     * is able to create an object. Afterwards, we can set attribute for this
     * object. This will cause NullPointer in Java! However, in C++, it works!
     * 2) For mapVersionning , we use value set by the
     * MappingSocketMsgHeader constructor.
     * TODO In MappingVersion mobility, maybe we should rethink how to set the
     * mapping versionning.
     * 3) In this case, maybe we don't need to care about MAP flags.
     */
    MappingSocketMsgHeader mapSockHeader;
    mapSockHeader.SetMapAddresses ((int) mapSockHeader.GetMapAddresses() | static_cast<int> (LispMappingSocket::MAPA_RLOC));
    // Question: why not consider LispMappingSocket::MAPA_EID, LispMappingSocket::MAPA_EIDMASK indicate implicitly the presence of EID?
    mapSockHeader.SetMapAddresses ((int) mapSockHeader.GetMapAddresses() | static_cast<int> (LispMappingSocket::MAPA_EIDMASK));

    /**
     *  This newly assigned RLOC-EID mapping should be inserted into lisp
     *  into database (instead of cache). It seems that in Lionel's implementation.
     *  No field in message header to indicate the message should be inserted into
     *  database or cache? Maybe in the method of LispOverIpv4 processing this message
     *  should explicitly to decide which database it should manipulate.
     */
    uint8_t buf[256];
    mapSockMsg->Serialize (buf);
    mapSockHeader.SetMapType (LispMappingSocket::MAPM_DATABASE_UPDATE);
    Ptr<Packet> packet = Create<Packet> (buf, 256);
    packet->AddHeader (mapSockHeader);
    m_lispMappingSocket->Send (packet);
    NS_LOG_DEBUG("Send newly assigned RLOC-EID to lispOverIpv4 so that LISP-MN's Database (not Cache) can be updated!");
    NS_LOG_DEBUG("RLOCs sent by DHCP client: \n"<<mapSockMsg->GetLocators()->Print());

  }

  void
  DhcpClient::RemoveAndStart ()
  {
    NS_LOG_FUNCTION_NOARGS ();
    Simulator::Remove (m_nextOfferEvent);
    Simulator::Remove (m_refreshEvent);
    Simulator::Remove (m_rebindEvent);
    Simulator::Remove (m_timeout);

    Ptr<Ipv4> ipv4MN = GetNode ()->GetObject<Ipv4> ();
    int32_t ifIndex = ipv4MN->GetInterfaceForDevice (
	GetNode ()->GetDevice (m_device));
    NS_ASSERT(ifIndex >= 0);
    for (uint32_t i = 0; i < ipv4MN->GetNAddresses (ifIndex); i++)
      {
	if (ipv4MN->GetAddress (ifIndex, i).GetLocal () == m_myAddress)
	  {
	    ipv4MN->RemoveAddress (ifIndex, i);
	    break;
	  }
      }
    m_expiry (m_myAddress);
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (
	ipv4MN);
    uint32_t i;
    for (i = 0; i < staticRouting->GetNRoutes (); i++)
      {
	if (staticRouting->GetRoute (i).GetGateway () == m_gateway
	    && staticRouting->GetRoute (i).GetInterface () == (uint32_t) ifIndex
	    && staticRouting->GetRoute (i).GetDest ()
		== Ipv4Address ("0.0.0.0"))
	  {
	    staticRouting->RemoveRoute (i);
	    break;
	  }
      }
    StartApplication ();
  }

} // Namespace ns3
