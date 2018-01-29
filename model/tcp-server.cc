/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 *  Copyright (c) 2007,2008,2009 INRIA, UDCAST
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
 * Author: SeyedAlireza SanaeeKohroudi <sarsaaee@comp.iust.ac.ir>
 *                                     <sarsanaee@gmail.com>
 */

#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "packet-loss-counter.h"

#include "seq-ts-header.h"
#include "tcp-server.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpServer");

NS_OBJECT_ENSURE_REGISTERED (TcpServer);


TypeId
TcpServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpServer")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<TcpServer> ()
    .AddAttribute ("Port",
                   "Port on which we listen for incoming packets.",
                   UintegerValue (100),
                   MakeUintegerAccessor (&TcpServer::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("Local", "The Address on which to Bind the rx socket.",
                   Ipv4AddressValue (),
                   MakeIpv4AddressAccessor (&TcpServer::m_local),
                   MakeIpv4AddressChecker ())
    .AddAttribute ("PacketWindowSize",
                   "The size of the window used to compute the packet loss. This value should be a multiple of 8.",
                   UintegerValue (32),
                   MakeUintegerAccessor (&TcpServer::GetPacketWindowSize,
                                         &TcpServer::SetPacketWindowSize),
                   MakeUintegerChecker<uint16_t> (8,256))
  ;
  return tid;
}

TcpServer::TcpServer ()
  : m_lossCounter (0)
{
  NS_LOG_FUNCTION (this);
  m_received=0;
}

void
TcpServer::Setup(Ipv4Address addr, uint16_t port)
{
  NS_LOG_FUNCTION (this << addr << port);
  m_port = port;
  m_local = addr;
}


TcpServer::~TcpServer ()
{
  NS_LOG_FUNCTION (this);
}

uint16_t
TcpServer::GetPacketWindowSize () const
{
  NS_LOG_FUNCTION (this);
  return m_lossCounter.GetBitMapSize ();
}

void
TcpServer::SetPacketWindowSize (uint16_t size)
{
  NS_LOG_FUNCTION (this << size);
  m_lossCounter.SetBitMapSize (size);
}

uint32_t
TcpServer::GetLost (void) const
{
  NS_LOG_FUNCTION (this);
  return m_lossCounter.GetLost ();
}

uint64_t
TcpServer::GetReceived (void) const
{
  NS_LOG_FUNCTION (this);
  return m_received;
}

void
TcpServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void
TcpServer::StartApplication (void)
{
  NS_LOG_FUNCTION (this);
  
  TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
  m_socket = Socket::CreateSocket (GetNode (), tid);
	InetSocketAddress local = InetSocketAddress (m_local, m_port);

  int res = m_socket->Bind (local);
  
  if (res == -1)
    {
      NS_FATAL_ERROR ("Failed to bind socket");
    }
    
	m_socket->Listen();
	NS_LOG_INFO("Echo Server local address:  " << m_local << " port: " << m_port << " bind: " << res );

  m_socket->SetRecvCallback (MakeCallback (&TcpServer::HandleRead, this));
  m_socket->SetCloseCallbacks(MakeCallback(&TcpServer::HandleClose, this), MakeCallback(&TcpServer::HandleClose, this));
  m_socket->SetAcceptCallback (
    //MakeCallback (&TcpEchoServer::HandleAcceptRequest, this),
    MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
    MakeCallback (&TcpServer::HandleAccept, this));
  NS_LOG_INFO("$$$$$$$$$$$$$$$$$$$$$$"); 
}

void 
TcpServer::HandleAccept (Ptr<Socket> s, const Address& from)
{
	NS_LOG_FUNCTION (this << s << from);
	NS_LOG_INFO("ACCEPT IN ECHO SERVER from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
	s->SetRecvCallback (MakeCallback (&TcpServer::HandleRead, this));
  m_neighbor_socket = s;
  //m_sendEvent = Simulator::Schedule (Seconds(5), &TcpServer::Send, this);
}

bool 
TcpServer::HandleAcceptRequest (Ptr<Socket> s, const Address& from)
{
	NS_LOG_INFO(" HANDLE ACCEPT REQUEST FROM " <<  InetSocketAddress::ConvertFrom(from));
	return true;
}

void
TcpServer::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void
TcpServer::HandleClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this);
  m_socket->Close();
}

void
TcpServer::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
  {
      if (packet->GetSize () > 0)
      {
          SeqTsHeader seqTs;
          packet->RemoveHeader (seqTs);
          uint32_t currentSequenceNumber = seqTs.GetSeq ();
          if (InetSocketAddress::IsMatchingType (from))
          {
              NS_LOG_INFO ("TraceDelay: RX " << packet->GetSize () <<
                           " bytes from "<< InetSocketAddress::ConvertFrom (from).GetIpv4 () <<
                           " Sequence Number: " << currentSequenceNumber <<
                           " Uid: " << packet->GetUid () <<
                           " TXtime: " << seqTs.GetTs () <<
                           " RXtime: " << Simulator::Now () <<
                           " Delay: " << Simulator::Now () - seqTs.GetTs ());
          }
          m_lossCounter.NotifyReceived (currentSequenceNumber);
          m_received++;
      }
  }
  m_sendEvent = Simulator::Schedule (Seconds(0.1), &TcpServer::Send, this);
}


void
TcpServer::Send(void)
{

  NS_LOG_FUNCTION (this);
  
  //NS_ASSERT (m_sendEvent.IsExpired ());
  //SeqTsHeader seqTs;
  //seqTs.SetSeq (m_sent);
  //Ptr<Packet> p = Create<Packet> (m_size-(8+4)); // 8+4 : the size of the seqTs header
  Ptr<Packet> p = Create<Packet> (100);
  //p->AddHeader (seqTs);

  //std::stringstream peerAddressStringStream;
  //peerAddressStringStream << Ipv4Address::ConvertFrom (m_peerAddress);
    
  int sendStatus = m_neighbor_socket->Send (p);
  //NS_LOG_UNCOND("sendStatus " << sendStatus);

  if (sendStatus >= 0)
  {
      /*
      NS_LOG_INFO ("TraceDelay TX " << m_size << " bytes to "
                                    << peerAddressStringStream.str () << " Uid: "
                                    << p->GetUid () << " Time: "
                                    << (Simulator::Now ()).GetSeconds ());
      */
      NS_LOG_INFO("sent");
  }
  else
  {
      NS_LOG_INFO ("Error while sending " << 100 << " bytes to " << "salam") ;//peerAddressStringStream.str ());
  }
  
}


} // Namespace ns3
