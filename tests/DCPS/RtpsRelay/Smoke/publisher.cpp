/*
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "Args.h"
#include "MessengerTypeSupportImpl.h"
#include "Writer.h"

#include <dds/DCPS/JsonValueWriter.h>
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/PublisherImpl.h>
#include <dds/DCPS/RTPS/RtpsDiscovery.h>
#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/transport/framework/TransportRegistry.h>
#include <dds/OpenddsDcpsExtTypeSupportImpl.h>

#ifdef ACE_AS_STATIC_LIBS
#  include <dds/DCPS/RTPS/RtpsDiscovery.h>
#  include <dds/DCPS/transport/rtps_udp/RtpsUdp.h>
#  ifdef OPENDDS_SECURITY
#    include <dds/DCPS/security/BuiltInPlugins.h>
#  endif
#endif
#ifdef OPENDDS_SECURITY
#  include <dds/DCPS/security/framework/Properties.h>
#endif

#include <ace/Get_Opt.h>
#include <ace/Log_Msg.h>
#include <ace/OS_NS_stdlib.h>
#include <ace/OS_NS_unistd.h>

#include <iostream>

#ifdef OPENDDS_SECURITY
const char auth_ca_file[] = "file:../../../security/certs/identity/identity_ca_cert.pem";
const char perm_ca_file[] = "file:../../../security/certs/permissions/permissions_ca_cert.pem";
const char id_cert_file[] = "file:../../../security/certs/identity/test_participant_02_cert.pem";
const char id_key_file[] = "file:../../../security/certs/identity/test_participant_02_private_key.pem";
const char governance_file[] = "file:./governance_signed.p7s";
const char permissions_file[] = "file:./permissions_publisher_signed.p7s";

void append(DDS::PropertySeq& props, const char* name, const char* value, bool propagate = false)
{
  const DDS::Property_t prop = {name, value, propagate};
  const unsigned int len = props.length();
  props.length(len + 1);
  props[len] = prop;
}
#endif

bool check_lease_recovery = false;
bool expect_unmatch = false;

const char USER_DATA[] = "The Publisher";

int ACE_TMAIN(int argc, ACE_TCHAR *argv[])
{
  DDS::DomainParticipantFactory_var dpf;
  DDS::DomainParticipant_var participant;

  try {

    std::cout << "Starting publisher" << std::endl;
    {
      // Initialize DomainParticipantFactory
      dpf = TheParticipantFactoryWithArgs(argc, argv);

      int status = EXIT_SUCCESS;
      if ((status = parse_args(argc, argv)) != EXIT_SUCCESS) {
        return status;
      }

      OpenDDS::DCPS::RcHandle<OpenDDS::DCPS::TransportInst> transport_inst = TheTransportRegistry->get_inst("pub_rtps");
      transport_inst->count_messages(true);

      DDS::DomainParticipantQos part_qos;
      dpf->get_default_participant_qos(part_qos);
      part_qos.user_data.value.length(static_cast<unsigned int>(std::strlen(USER_DATA)));
      std::memcpy(part_qos.user_data.value.get_buffer(), USER_DATA, std::strlen(USER_DATA));

#if defined(OPENDDS_SECURITY)
      if (TheServiceParticipant->get_security()) {
        DDS::PropertySeq& props = part_qos.property.value;
        append(props, DDS::Security::Properties::AuthIdentityCA, auth_ca_file);
        append(props, DDS::Security::Properties::AuthIdentityCertificate, id_cert_file);
        append(props, DDS::Security::Properties::AuthPrivateKey, id_key_file);
        append(props, DDS::Security::Properties::AccessPermissionsCA, perm_ca_file);
        append(props, DDS::Security::Properties::AccessGovernance, governance_file);
        append(props, DDS::Security::Properties::AccessPermissions, permissions_file);
      }
#endif

      // Create DomainParticipant
      participant = dpf->create_participant(42,
                                part_qos,
                                DDS::DomainParticipantListener::_nil(),
                                OpenDDS::DCPS::DEFAULT_STATUS_MASK);

      if (CORBA::is_nil(participant.in())) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("%N:%l: main()")
                          ACE_TEXT(" ERROR: create_participant failed!\n")),
                         1);
      }

      OpenDDS::DCPS::DomainParticipantImpl* dp_impl =
        dynamic_cast<OpenDDS::DCPS::DomainParticipantImpl*>(participant.in());

      OpenDDS::DCPS::RcHandle<OpenDDS::RTPS::RtpsDiscovery> disc = OpenDDS::DCPS::static_rchandle_cast<OpenDDS::RTPS::RtpsDiscovery>(TheServiceParticipant->get_discovery(42));
      const OpenDDS::DCPS::GUID_t guid = dp_impl->get_id();
      OpenDDS::DCPS::RcHandle<OpenDDS::DCPS::TransportInst> discovery_inst = disc->sedp_transport_inst(42, guid);
      discovery_inst->count_messages(true);

      // Register TypeSupport (Messenger::Message)
      Messenger::MessageTypeSupport_var mts =
        new Messenger::MessageTypeSupportImpl();

      if (mts->register_type(participant.in(), "") != DDS::RETCODE_OK) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("%N:%l: main()")
                          ACE_TEXT(" ERROR: register_type failed!\n")),
                         1);
      }

      // Create Topic
      CORBA::String_var type_name = mts->get_type_name();
      DDS::Topic_var topic =
        participant->create_topic("Movie Discussion List",
                                  type_name.in(),
                                  TOPIC_QOS_DEFAULT,
                                  DDS::TopicListener::_nil(),
                                  OpenDDS::DCPS::DEFAULT_STATUS_MASK);

      if (CORBA::is_nil(topic.in())) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("%N:%l: main()")
                          ACE_TEXT(" ERROR: create_topic failed!\n")),
                         1);
      }

      // Create Publisher
      DDS::PublisherQos publisher_qos;
      participant->get_default_publisher_qos(publisher_qos);
      publisher_qos.partition.name.length(1);
      publisher_qos.partition.name[0] = "OCI";

      DDS::Publisher_var pub =
        participant->create_publisher(publisher_qos,
                                      DDS::PublisherListener::_nil(),
                                      OpenDDS::DCPS::DEFAULT_STATUS_MASK);

      if (CORBA::is_nil(pub.in())) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("%N:%l: main()")
                          ACE_TEXT(" ERROR: create_publisher failed!\n")),
                         1);
      }

      DDS::DataWriterQos qos;
      pub->get_default_datawriter_qos(qos);
      std::cout << "Reliable DataWriter" << std::endl;
      qos.history.kind = DDS::KEEP_ALL_HISTORY_QOS;
      qos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;

      // Create DataWriter
      DDS::DataWriter_var dw =
        pub->create_datawriter(topic.in(),
                               qos,
                               DDS::DataWriterListener::_nil(),
                               OpenDDS::DCPS::DEFAULT_STATUS_MASK);

      if (CORBA::is_nil(dw.in())) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("%N:%l: main()")
                          ACE_TEXT(" ERROR: create_datawriter failed!\n")),
                         1);
      }

      // Start writing threads
      std::cout << "Creating Writer" << std::endl;
      Writer* writer = check_lease_recovery ? new Writer(dw.in(), 30, true) : new Writer(dw.in(), 10, false);
      std::cout << "Starting Writer" << std::endl;
      writer->start();

      while (!writer->is_finished()) {
        ACE_Time_Value small_time(0, 250000);
        ACE_OS::sleep(small_time);
      }

      std::cout << "Writer finished " << std::endl;
      writer->end();

      std::cout << "Writer wait for ACKS" << std::endl;

      DDS::Duration_t timeout =
        { DDS::DURATION_INFINITE_SEC, DDS::DURATION_INFINITE_NSEC };
      dw->wait_for_acknowledgments(timeout);

      std::cerr << "deleting DW" << std::endl;
      delete writer;

#if OPENDDS_HAS_JSON_VALUE_WRITER
      std::cout << "Publisher Guid: " << OpenDDS::DCPS::LogGuid(guid).c_str() << std::endl;
      OpenDDS::DCPS::TransportStatisticsSequence stats;
      disc->append_transport_statistics(42, guid, stats);
      transport_inst->append_transport_statistics(stats);

      for (unsigned int i = 0; i != stats.length(); ++i) {
        std::cout << "Publisher Transport Statistics: " << OpenDDS::DCPS::to_json(stats[i]) << std::endl;
      }
#endif
    }

    // Clean-up!
    std::cerr << "deleting contained entities" << std::endl;
    participant->delete_contained_entities();
    std::cerr << "deleting participant" << std::endl;
    dpf->delete_participant(participant.in());
    std::cerr << "shutdown" << std::endl;
    TheServiceParticipant->shutdown();

  } catch (const CORBA::Exception& e) {
    e._tao_print_exception("Exception caught in main():");
    ACE_OS::exit(1);
  }

  return 0;
}
