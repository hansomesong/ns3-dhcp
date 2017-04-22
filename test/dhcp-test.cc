/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * Authors: Ankit Deepak <adadeepak8@gmail.com>
 *          Deepti Rajagopal <deeptir96@gmail.com>
 *
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

#include "ns3/test.h"

using namespace ns3;

class DhcpTestCase1 : public TestCase
{
public:
  DhcpTestCase1 ();
  virtual ~DhcpTestCase1 ();

private:
  virtual void DoRun (void);
};

DhcpTestCase1::DhcpTestCase1 ()
  : TestCase ("Dhcp test case ")
{
}

DhcpTestCase1::~DhcpTestCase1 ()
{
}

void
DhcpTestCase1::DoRun (void)
{
  /*Set up devices*/
  NodeContainer MN;
  NodeContainer Router;
  MN.Create (3);
  Router.Create (2);

  NodeContainer net (MN, Router);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("2ms"));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  NetDeviceContainer dev_net = csma.Install (net);

  InternetStackHelper tcpip;
  tcpip.Install (MN);
  tcpip.Install (Router);

  Ptr<Ipv4> ipv4MN = net.Get (0)->GetObject<Ipv4> ();
  uint32_t ifIndex = ipv4MN->AddInterface (dev_net.Get (0));
  ipv4MN->SetForwarding (ifIndex, true);
  ipv4MN->SetUp (ifIndex);

  Ptr<Ipv4> ipv4MN1 = net.Get (1)->GetObject<Ipv4> ();
  uint32_t ifIndex1 = ipv4MN1->AddInterface (dev_net.Get (1));
  ipv4MN1->SetForwarding (ifIndex1, true);
  ipv4MN1->SetUp (ifIndex1);

  Ptr<Ipv4> ipv4MN2 = net.Get (2)->GetObject<Ipv4> ();
  uint32_t ifIndex2 = ipv4MN2->AddInterface (dev_net.Get (2));
  ipv4MN2->SetForwarding (ifIndex2, true);
  ipv4MN2->SetUp (ifIndex2);

  Ptr<Ipv4> ipv4Router = net.Get (3)->GetObject<Ipv4> ();
  ifIndex = ipv4Router->AddInterface (dev_net.Get (3));
  ipv4Router->AddAddress (ifIndex, Ipv4InterfaceAddress (Ipv4Address ("172.30.0.12"), Ipv4Mask ("/0"))); // need to remove this workaround
  ipv4Router->AddAddress (ifIndex, Ipv4InterfaceAddress (Ipv4Address ("172.30.0.12"), Ipv4Mask ("/24")));
  ipv4Router->SetForwarding (ifIndex, true);
  ipv4Router->SetUp (ifIndex);

  Ptr<Ipv4> ipv4Router1 = net.Get (4)->GetObject<Ipv4> ();
  ifIndex = ipv4Router1->AddInterface (dev_net.Get (4));
  ipv4Router1->AddAddress (ifIndex, Ipv4InterfaceAddress (Ipv4Address ("173.30.0.12"), Ipv4Mask ("/0"))); // need to remove this workaround
  ipv4Router1->AddAddress (ifIndex, Ipv4InterfaceAddress (Ipv4Address ("173.30.0.12"), Ipv4Mask ("/24")));
  ipv4Router1->SetForwarding (ifIndex, true);
  ipv4Router1->SetUp (ifIndex);

  DhcpServerHelper dhcp_server (Ipv4Address ("172.30.0.0"), Ipv4Mask ("/24"), Ipv4Address ("172.30.0.12"), Ipv4Address ("172.30.0.10"), Ipv4Address ("172.30.0.100"));
  ApplicationContainer ap_dhcp_server = dhcp_server.Install (Router.Get (0));
  ap_dhcp_server.Start (Seconds (1.0));
  ap_dhcp_server.Stop (Seconds (500.0));

  DhcpServerHelper dhcp_server1 (Ipv4Address ("173.30.0.0"), Ipv4Mask ("/24"), Ipv4Address ("173.30.0.12"), Ipv4Address ("173.30.0.10"), Ipv4Address ("173.30.0.100"));
  ApplicationContainer ap_dhcp_server1 = dhcp_server1.Install (Router.Get (1));
  ap_dhcp_server1.Start (Seconds (1.0));
  ap_dhcp_server1.Stop (Seconds (500.0));

  DhcpClientHelper dhcp_client (0);
  ApplicationContainer ap_dhcp_client = dhcp_client.Install (MN.Get (0));
  ap_dhcp_client.Start (Seconds (1.0));
  ap_dhcp_client.Stop (Seconds (100.0));

  ApplicationContainer ap_dhcp_client3 = dhcp_client.Install (MN.Get (0));
  ap_dhcp_client3.Start (Seconds (400.0));
  ap_dhcp_client3.Stop (Seconds (500.0));

  DhcpClientHelper dhcp_client1 (0);
  ApplicationContainer ap_dhcp_client1 = dhcp_client1.Install (MN.Get (1));
  ap_dhcp_client1.Start (Seconds (1.0));
  ap_dhcp_client1.Stop (Seconds (500.0));

  DhcpClientHelper dhcp_client2 (0);
  ApplicationContainer ap_dhcp_client2 = dhcp_client2.Install (MN.Get (2));
  ap_dhcp_client2.Start (Seconds (1.0));
  ap_dhcp_client2.Stop (Seconds (500.0));

  Simulator::Stop (Seconds (500.0));

  Simulator::Run ();

  Ipv4Address address = ipv4MN->GetAddress (ifIndex, 0).GetLocal ();
  NS_TEST_ASSERT_MSG_EQ (Ipv4Address ("172.30.0.11"),address,address);

  Ipv4Address address1 = ipv4MN1->GetAddress (ifIndex, 0).GetLocal ();
  NS_TEST_ASSERT_MSG_EQ (Ipv4Address ("172.30.0.14"),address1,address1);

  Ipv4Address address2 = ipv4MN2->GetAddress (ifIndex, 0).GetLocal ();
  NS_TEST_ASSERT_MSG_EQ (Ipv4Address ("172.30.0.13"),address2,address2);

  Simulator::Destroy ();
}

class DhcpTestSuite : public TestSuite
{
public:
  DhcpTestSuite ();
};

DhcpTestSuite::DhcpTestSuite ()
  : TestSuite ("dhcp", UNIT)
{
  AddTestCase (new DhcpTestCase1, TestCase::QUICK);
}

static DhcpTestSuite dhcpTestSuite;

