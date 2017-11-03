/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 Natale Patriciello <natale.patriciello@gmail.com>
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
 */

#include "tcp-general-test.h"
#include "ns3/node.h"
#include "ns3/log.h"
#include "ns3/config.h"
#include "tcp-error-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpPktsAckedTestSuite");

class DummyCongControl;

/**
 * \ingroup internet-test
 * \ingroup tests
 *
 * \brief Check the number of times that PktsAcked is called
 *
 * Set a custom congestion control class, which calls PktsAckedCalled
 * each time the TCP implementation calls PktsAcked.
 *
 * The checks are performed in FinalChecks: the number of bytes acked divided
 * by segment size should be the same as the number of segments passed through
 * PktsAcked in the congestion control.
 *
 * \see DummyCongControl
 * \see FinalChecks
 */
class TcpPktsAckedTest : public TcpGeneralTest
{
public:
  /**
   * \brief Constructor.
   * \param desc Test description.
   */
  TcpPktsAckedTest (const std::string &desc,
                    std::vector<uint32_t> &toDrop);

  /**
   * \brief Called when an ACK is received.
   * \param segmentsAcked The segment ACKed.
   */
  void PktsAckedCalled (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);

protected:
  virtual Ptr<TcpSocketMsgBase> CreateSenderSocket (Ptr<Node> node);
  virtual Ptr<ErrorModel> CreateReceiverErrorModel ();
  virtual void Rx (const Ptr<const Packet> p, const TcpHeader&h, SocketWho who);

  virtual void ConfigureEnvironment ();

  void FinalChecks ();

private:
  uint32_t m_segmentsAcked;    //!< Contains the number of times PktsAcked is called
  uint32_t m_bytesReceived; //!< Contains the ack number received

  Ptr<DummyCongControl> m_congCtl; //!< Dummy congestion control.
  std::vector<uint32_t> m_toDrop;     //!< List of SequenceNumber to drop
};

/**
 * \ingroup internet-test
 * \ingroup tests
 *
 * \brief Behaves as NewReno, except that each time PktsAcked is called,
 * a notification is sent to TcpPktsAckedTest.
 */
class DummyCongControl : public TcpNewReno
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  DummyCongControl ()
  {
  }

  /**
   * \brief Set the callback to be used when an ACK is received.
   * \param test The callback.
   */
  void SetCallback (Callback<void, Ptr<TcpSocketState>, uint32_t> test)
  {
    m_test = test;
  }

  void PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                  const Time& rtt)
  {
    m_test (tcb, segmentsAcked);
  }

private:
  Callback<void, Ptr<TcpSocketState>, uint32_t> m_test; //!< Callback to be used when an ACK is received.
};

TypeId
DummyCongControl::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DummyCongControl")
    .SetParent<TcpNewReno> ()
    .AddConstructor<DummyCongControl> ()
    .SetGroupName ("Internet")
  ;
  return tid;
}

TcpPktsAckedTest::TcpPktsAckedTest (const std::string &desc,
                                    std::vector<uint32_t> &toDrop)
  : TcpGeneralTest (desc),
    m_segmentsAcked (0),
    m_bytesReceived (0),
    m_toDrop (toDrop)
{
}

void
TcpPktsAckedTest::ConfigureEnvironment ()
{
  TcpGeneralTest::ConfigureEnvironment ();
  SetAppPktCount (20);
  SetMTU (500);
  Config::SetDefault ("ns3::TcpSocket::DelAckTimeout", TimeValue (Seconds (0)));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
  Config::SetDefault ("ns3::TcpSocketBase::Sack", BooleanValue (false));
}

Ptr<TcpSocketMsgBase>
TcpPktsAckedTest::CreateSenderSocket (Ptr<Node> node)
{
  Ptr<TcpSocketMsgBase> s = TcpGeneralTest::CreateSenderSocket (node);
  m_congCtl = CreateObject<DummyCongControl> ();
  m_congCtl->SetCallback (MakeCallback (&TcpPktsAckedTest::PktsAckedCalled, this));
  s->SetCongestionControlAlgorithm (m_congCtl);

  return s;
}

void
TcpPktsAckedTest::PktsAckedCalled (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
        m_segmentsAcked += segmentsAcked;
}

void
TcpPktsAckedTest::Rx (const Ptr<const Packet> p, const TcpHeader &h, SocketWho who)
{
  if (who == SENDER && (!(h.GetFlags () & TcpHeader::SYN)))
    {
      m_bytesReceived = h.GetAckNumber ().GetValue ();
    }
}

void
TcpPktsAckedTest::FinalChecks ()
{
  std::cout << "m_segsReceived:" << m_bytesReceived / GetSegSize (SENDER) << "\tm_segmentsAcked " << m_segmentsAcked << std::endl;
  NS_TEST_ASSERT_MSG_EQ (m_bytesReceived / GetSegSize (SENDER), m_segmentsAcked,
                         "Not all acked segments have been passed to PktsAcked method");
}

Ptr<ErrorModel>
TcpPktsAckedTest::CreateReceiverErrorModel ()
{
  Ptr<TcpSeqErrorModel> m_errorModel = CreateObject<TcpSeqErrorModel> ();
  for (std::vector<uint32_t>::iterator it = m_toDrop.begin (); it != m_toDrop.end (); ++it)
    {
      m_errorModel->AddSeqToKill (SequenceNumber32 (*it));
    }

  return m_errorModel;
}

/**
 * \ingroup internet-test
 * \ingroup tests
 *
 * \brief PktsAcked is calls TestSuite.
 */
class TcpPktsAckedTestSuite : public TestSuite
{
public:
  TcpPktsAckedTestSuite () : TestSuite ("tcp-pkts-acked-test", UNIT)
  {
    std::vector<uint32_t> toDrop;
    AddTestCase (new TcpPktsAckedTest ("PktsAcked check while in OPEN state", toDrop),
                 TestCase::QUICK);
    // Add DISORDER, RECOVERY and LOSS state check
    toDrop.push_back (2001);
    AddTestCase (new TcpPktsAckedTest ("PktsAcked check while in all the states", toDrop),
                 TestCase::QUICK);
  }
};

static TcpPktsAckedTestSuite g_TcpPktsAckedTestSuite; //!< Static variable for test initialization

