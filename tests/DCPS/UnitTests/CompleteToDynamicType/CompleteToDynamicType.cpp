#include "CompleteToDynamicTypeTypeSupportImpl.h"
#include <map>
#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/XTypes/TypeObject.h>
#include <dds/DCPS/XTypes/DynamicType.h>
#include <dds/DCPS/XTypes/DynamicTypeMember.h>
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/TopicDescriptionImpl.h>
#include <dds/DCPS/PublisherImpl.h>
#include <dds/DCPS/DomainParticipantImpl.h>
#include <dds/DCPS/Qos_Helper.h>
#include <dds/DCPS/transport/framework/TransportRegistry.h>

#include <gtest/gtest.h>

using namespace OpenDDS;

XTypes::TypeLookupService tls;

template<typename T>
void test_conversion(const XTypes::DynamicType_rch& expected_dynamic_type)
{
  const XTypes::TypeIdentifier& com_ti = DCPS::getCompleteTypeIdentifier<T>();
  const XTypes::TypeMap& com_map = DCPS::getCompleteTypeMap<T>();
  XTypes::TypeMap::const_iterator pos = com_map.find(com_ti);
  EXPECT_TRUE(pos != com_map.end());
  const XTypes::TypeObject& com_to = pos->second;
  XTypes::DynamicType_rch converted_dt(new XTypes::DynamicType, OpenDDS::DCPS::keep_count());
  tls.complete_to_dynamic(converted_dt, com_to.complete);
  bool f = (*expected_dynamic_type.in() == *converted_dt.in());
  EXPECT_EQ(*expected_dynamic_type.in(), *converted_dt.in());
}

TEST(CompleteToMinimalTypeObject, MyStruct)
{
  XTypes::DynamicType_rch expected_dt(new XTypes::DynamicType, OpenDDS::DCPS::keep_count());
  XTypes::TypeDescriptor* td(new XTypes::TypeDescriptor);
  td->kind = XTypes::TK_STRUCTURE;
  td->name = "MyStruct";
  td->bound.length(0);
  td->extensibility_kind = XTypes::APPENDABLE;
  td->is_nested = 0;
  expected_dt->descriptor_ = td;
  XTypes::DynamicTypeMember_rch expected_dtm(new XTypes::DynamicTypeMember, OpenDDS::DCPS::keep_count());
  XTypes::MemberDescriptor* md(new XTypes::MemberDescriptor);
  expected_dtm->descriptor_ = md;
  md->name = "l";
  md->id = 0;
  md->index = 0;
  md->try_construct_kind = XTypes::DISCARD;
  XTypes::DynamicType_rch long_expected_dt(new XTypes::DynamicType, OpenDDS::DCPS::keep_count());
  XTypes::TypeDescriptor* long_td(new XTypes::TypeDescriptor);
  long_td->kind = XTypes::TK_INT32;
  long_td->bound.length(0);
  long_td->name = "long";
  long_expected_dt->descriptor_ = long_td;
  md->type = long_expected_dt;
  expected_dt->member_by_index.insert(expected_dt->member_by_index.end(), expected_dtm);
  expected_dt->member_by_id.insert(expected_dt->member_by_id.end(), std::make_pair(md->id , expected_dtm));
  expected_dt->member_by_name.insert(expected_dt->member_by_name.end(), std::make_pair(md->name , expected_dtm));
  test_conversion<DCPS::MyMod_MyStruct_xtag>(expected_dt);
}

int main(int argc, char* argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  XTypes::TypeIdentifierPairSeq tid_pairs;

  XTypes::TypeIdentifierPair my_struct_tids;
  my_struct_tids.type_identifier1 = DCPS::getCompleteTypeIdentifier<DCPS::MyMod_MyStruct_xtag>();
  my_struct_tids.type_identifier2 = DCPS::getMinimalTypeIdentifier<DCPS::MyMod_MyStruct_xtag>();
  tid_pairs.append(my_struct_tids);

  tls.update_type_identifier_map(tid_pairs);

  return RUN_ALL_TESTS();
}
