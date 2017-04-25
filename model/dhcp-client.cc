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

#include "dhcp-client.h"
#include "dhcp-server.h"
#include "dhcp-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DhcpClient");
NS_OBJECT_ENSURE_REGISTERED (DhcpClient);

TypeId
DhcpClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DhcpClient")
    .SetParent<Application> ()
    .AddConstructor<DhcpClient> ()
    .SetGroupName ("Internet-Apps")
    .AddAttribute ("NetDevice", "Index of netdevice of the node for DHCP",
                   UintegerValue (0),
                   MakeUintegerAccessor (&DhcpClient::m_device),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("RTRS", "Time for retransmission of Discover message",
                   TimeValue (Seconds (5)),
                   MakeTimeAccessor (&DhcpClient::m_rtrs),
                   MakeTimeChecker ())
    .AddAttribute ("Collect", "Time for which offer collection starts",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&DhcpClient::m_collect),
                   MakeTimeChecker ())
    .AddAttribute ("ReRequest", "Time after which request will be resent to next server",
                   TimeValue (Seconds (10)),
                   MakeTimeAccessor (&DhcpClient::m_nextoffer),
                   MakeTimeChecker ())
    .AddAttribute ("Transactions",
                   "The possible value of transaction numbers ",
                   StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1000000.0]"),
                   MakePointerAccessor (&DhcpClient::m_ran),
                   MakePointerChecker<RandomVariableStream> ())
    .AddTraceSource ("NewLease",
                     "Get a NewLease",
                     MakeTraceSourceAccessor (&DhcpClient::m_newLease),
                     "ns3::Ipv4Address::TracedCallback")
    .AddTraceSource ("Expiry",
                     "A lease expires",
                     MakeTraceSourceAccessor (&DhcpClient::m_expiry),
                     "ns3::Ipv4Address::TracedCallback");
  return tid;
}

DhcpClient::DhcpClient () : m_server (Ipv4Address::GetAny ())
{
  NS_LOG_FUNCTION_NOARGS ();
  m_socket = 0;
  m_refreshEvent = EventId ();
  m_requestEvent = EventId ();
  m_discoverEvent = EventId ();
  m_rebindEvent = EventId ();
  m_nextOfferEvent = EventId ();
  m_timeout = EventId ();
}

DhcpClient::~DhcpClient ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

Ipv4Address DhcpClient::GetDhcpServer (void)
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
  NS_LOG_FUNCTION (this << stream);
  m_ran->SetStream (stream);
  return 1;
}

void DhcpClient::CreateSocket ()
{
  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::Ipv4RawSocketFactory");
      m_socket = DynamicCast <Ipv4RawSocketImpl> (Socket::CreateSocket (GetNode (), tid));
      m_socket->SetProtocol (UdpL4Protocol::PROT_NUMBER);
      m_socket->SetAllowBroadcast (true);
      m_socket->Bind ();
      m_socket->BindToNetDevice (GetNode ()->GetDevice (m_device));
    }
  m_socket->SetRecvCallback (MakeCallback (&DhcpClient::NetHandler, this));
}

void DhcpClient::CloseSocket ()
{
  m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
  m_socket->Close ();
}

void
DhcpClient::StartApplication (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_remoteAddress = Ipv4Address ("255.255.255.255");
  m_myAddress = Ipv4Address ("0.0.0.0");
  m_gateway = Ipv4Address ("0.0.0.0");
  Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();
  uint32_t ifIndex = ipv4->GetInterfaceForDevice (GetNode ()->GetDevice (m_device));
  uint32_t i;
  bool found = false;
  for (i = 0; i < ipv4->GetNAddresses (ifIndex); i++)
    {
      if (ipv4->GetAddress (ifIndex,i).GetLocal () == m_myAddress)
        {
          found = true;
        }
    }
  if (!found)
    {
      ipv4->AddAddress (ifIndex, Ipv4InterfaceAddress (Ipv4Address ("0.0.0.0"),Ipv4Mask ("/0")));
    }
  CreateSocket ();
  GetNode ()->GetDevice (m_device)->AddLinkChangeCallback (MakeCallback (&DhcpClient::LinkStateHandler, this));
  Boot ();
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

  int32_t ifIndex = ipv4->GetInterfaceForDevice (GetNode ()->GetDevice (m_device));
  NS_ASSERT (ifIndex >= 0);
  for (uint32_t i = 0; i < ipv4->GetNAddresses (ifIndex); i++)
    {
      if (ipv4->GetAddress (ifIndex,i).GetLocal () == m_myAddress)
        {
          ipv4->RemoveAddress (ifIndex, i);
          break;
        }
    }

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (ipv4);
  uint32_t i;
  for (i = 0; i < staticRouting->GetNRoutes (); i++)
    {
      if (staticRouting->GetRoute (i).GetGateway () == m_gateway && staticRouting->GetRoute (i).GetInterface () == (uint32_t) ifIndex && staticRouting->GetRoute (i).GetDest () == Ipv4Address ("0.0.0.0"))
        {
          staticRouting->RemoveRoute (i);
          break;
        }
    }
  CloseSocket ();
}

void DhcpClient::LinkStateHandler (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  if (GetNode ()->GetDevice (m_device)->IsLinkUp ())
    {
      NS_LOG_INFO ("Link up at " << Simulator::Now ().GetSeconds ());
      StartApplication ();
    }
  else
    {
      NS_LOG_INFO ("Link down at " << Simulator::Now ().GetSeconds ()); //reinitialization
      Simulator::Remove (m_refreshEvent); //stop refresh timer!!!!
      Simulator::Remove (m_rebindEvent);
      Simulator::Remove (m_timeout);
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());  //stop receiving on this socket !!!

      Ptr<Ipv4> ipv4MN = GetNode ()->GetObject<Ipv4> ();
      int32_t ifIndex = ipv4MN->GetInterfaceForDevice (GetNode ()->GetDevice (m_device));
      NS_ASSERT (ifIndex >= 0);
      for (uint32_t i = 0; i < ipv4MN->GetNAddresses (ifIndex); i++)
        {
          if (ipv4MN->GetAddress (ifIndex,i).GetLocal () == m_myAddress)
            {
              ipv4MN->RemoveAddress (ifIndex, i);
              break;
            }
        }

      Ipv4StaticRoutingHelper ipv4RoutingHelper;
      Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (ipv4MN);
      uint32_t i;
      for (i = 0; i < staticRouting->GetNRoutes (); i++)
        {
          if (staticRouting->GetRoute (i).GetGateway () == m_gateway && staticRouting->GetRoute (i).GetInterface () == (uint32_t) ifIndex && staticRouting->GetRoute (i).GetDest () == Ipv4Address ("0.0.0.0"))
            {
              staticRouting->RemoveRoute (i);
              break;
            }
        }
    }
}

void DhcpClient::NetHandler (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
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
      AcceptAck (header,from);
    }
  if (m_state == WAIT_ACK && header.GetType () == DhcpHeader::DHCPNACK)
    {
      Simulator::Remove (m_nextOfferEvent);
      Boot ();
    }
}

void DhcpClient::Boot (void)
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
  if(Node::ChecksumEnabled ())
    {
      udpHeader.EnableChecksums ();
      udpHeader.InitializeChecksum (m_myAddress,
                                    Ipv4Address ("255.255.255.255"),
                                    UdpL4Protocol::PROT_NUMBER);
    }
  packet->AddHeader (udpHeader);

  if ((m_socket->SendTo (packet, 0, InetSocketAddress (Ipv4Address ("255.255.255.255")))) >= 0)
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s DHCP DISCOVER sent");
    }
  else
    {
      NS_LOG_INFO ("Error while sending DHCP DISCOVER to " << m_remoteAddress);
    }
  m_state = WAIT_OFFER;
  m_offered = false;
  m_discoverEvent = Simulator::Schedule (m_rtrs, &DhcpClient::Boot, this);
}

void DhcpClient::OfferHandler (DhcpHeader header)
{
  m_offerList.push_back (header);
  if (m_offered == false)
    {
      Simulator::Remove (m_discoverEvent);
      m_offered = true;
      Simulator::Schedule (m_collect, &DhcpClient::Select, this);
    }
}

void DhcpClient::Select (void)
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
  NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s DHCP client sent DHCP REQUEST to DHCP Server");
}

void DhcpClient::Request (void)
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
      if(Node::ChecksumEnabled ())
        {
          udpHeader.EnableChecksums ();
          udpHeader.InitializeChecksum (m_myAddress,
                                        Ipv4Address ("255.255.255.255"),
                                        UdpL4Protocol::PROT_NUMBER);
        }
      packet->AddHeader (udpHeader);

      m_socket->SendTo (packet, 0, InetSocketAddress (Ipv4Address ("255.255.255.255")));
      m_state = WAIT_ACK;
      m_nextOfferEvent = Simulator::Schedule (m_nextoffer, &DhcpClient::Select, this);
    }
  else
    {
      //CreateSocket ();
      uint32_t addr = m_myAddress.Get ();
      packet = Create<Packet> ((uint8_t*)&addr, sizeof(addr));

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
      if(Node::ChecksumEnabled ())
        {
          udpHeader.EnableChecksums ();
          udpHeader.InitializeChecksum (m_myAddress,
                                        m_remoteAddress,
                                        UdpL4Protocol::PROT_NUMBER);
        }
      packet->AddHeader (udpHeader);

      if ((m_socket->SendTo (packet, 0, InetSocketAddress (m_remoteAddress))) >= 0)
        {
          NS_LOG_INFO ("DHCP REQUEST sent");
        }
      else
        {
          NS_LOG_INFO ("Error while sending DHCP REQ to " << m_remoteAddress);
        }
      m_state = WAIT_ACK;
    }
}

void DhcpClient::AcceptAck (DhcpHeader header, Address from)
{
  Simulator::Remove (m_rebindEvent);
  Simulator::Remove (m_refreshEvent);
  Simulator::Remove (m_timeout);
  NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () <<"s DHCP ACK received");
  Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();
  int32_t ifIndex = ipv4->GetInterfaceForDevice (GetNode ()->GetDevice (m_device));

  for (uint32_t i = 0; i < ipv4->GetNAddresses (ifIndex); i++)
    {
      if (ipv4->GetAddress (ifIndex,i).GetLocal () == m_myAddress)
        {
          ipv4->RemoveAddress (ifIndex, i);
          break;
        }
    }

  ipv4->AddAddress (ifIndex, Ipv4InterfaceAddress ((Ipv4Address)m_offeredAddress, m_myMask));
  ipv4->SetUp (ifIndex);

  if (m_myAddress != m_offeredAddress)
    {
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
      Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (ipv4);
      staticRouting->SetDefaultRoute (m_gateway, ifIndex, 0);
    }

  m_remoteAddress = InetSocketAddress::ConvertFrom (from).GetIpv4 ();
  NS_LOG_INFO ("Current DHCP Server is " << m_remoteAddress);

  m_offerList.clear ();
  m_refreshEvent = Simulator::Schedule (m_renew, &DhcpClient::Request, this);
  m_rebindEvent = Simulator::Schedule (m_rebind, &DhcpClient::Request, this);
  m_timeout =  Simulator::Schedule (m_lease, &DhcpClient::RemoveAndStart, this);
  m_state = REFRESH_LEASE;

  //Simulator::ScheduleNow(&DhcpClient::CloseSocket, this);
}

void DhcpClient::RemoveAndStart ()
{
  NS_LOG_FUNCTION_NOARGS ();
  Simulator::Remove (m_nextOfferEvent);
  Simulator::Remove (m_refreshEvent);
  Simulator::Remove (m_rebindEvent);
  Simulator::Remove (m_timeout);

  Ptr<Ipv4> ipv4MN = GetNode ()->GetObject<Ipv4> ();
  int32_t ifIndex = ipv4MN->GetInterfaceForDevice (GetNode ()->GetDevice (m_device));
  NS_ASSERT (ifIndex >= 0);
  for (uint32_t i = 0; i < ipv4MN->GetNAddresses (ifIndex); i++)
    {
      if (ipv4MN->GetAddress (ifIndex,i).GetLocal () == m_myAddress)
        {
          ipv4MN->RemoveAddress (ifIndex, i);
          break;
        }
    }
  m_expiry (m_myAddress);
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (ipv4MN);
  uint32_t i;
  for (i = 0; i < staticRouting->GetNRoutes (); i++)
    {
      if (staticRouting->GetRoute (i).GetGateway () == m_gateway && staticRouting->GetRoute (i).GetInterface () == (uint32_t) ifIndex && staticRouting->GetRoute (i).GetDest () == Ipv4Address ("0.0.0.0"))
        {
          staticRouting->RemoveRoute (i);
          break;
        }
    }
  StartApplication ();
}

} // Namespace ns3
