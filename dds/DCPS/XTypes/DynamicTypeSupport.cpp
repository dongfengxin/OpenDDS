/*
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include <DCPS/DdsDcps_pch.h>

#ifndef OPENDDS_SAFETY_PROFILE
#  include "DynamicTypeSupport.h"

#  include "DynamicTypeImpl.h"
#  include "Utils.h"
#  include "DynamicDataImpl.h"

#  include <dds/DCPS/debug.h>
#  include <dds/DCPS/DCPS_Utils.h>

OPENDDS_BEGIN_VERSIONED_NAMESPACE_DECL
namespace OpenDDS {
namespace XTypes {

using namespace OpenDDS::DCPS;

bool DynamicSample::serialize(Serializer& ser) const
{
  const DynamicDataImpl* const ddi = dynamic_cast<DynamicDataImpl*>(data_.in());
  if (!ddi) {
    if (log_level >= LogLevel::Notice) {
      ACE_ERROR((LM_NOTICE, "(%P|%t) NOTICE: DynamicSample::serialize: "
        "currently we only support DynamicDataImpl, the kind supplied by DynamicDataFactory\n"));
    }
    return false;
  }
  return ser << *ddi;
}

bool DynamicSample::deserialize(Serializer& ser)
{
  ACE_UNUSED_ARG(ser);
  // TODO
  return false;
}

size_t DynamicSample::serialized_size(const Encoding& enc) const
{
  const DynamicDataImpl* const ddi = dynamic_cast<DynamicDataImpl*>(data_.in());
  if (!ddi) {
    if (log_level >= LogLevel::Warning) {
      ACE_ERROR((LM_WARNING, "(%P|%t) WARNING: DynamicSample::serialized_size: "
        "currently we only support DynamicDataImpl, the kind supplied by DynamicDataFactory\n"));
    }
    return 0;
  }
  return DCPS::serialized_size(enc, *ddi);
}

bool DynamicSample::compare(const Sample& other) const
{
  ACE_UNUSED_ARG(other);
  // TODO
  return false;
}

} // namespace XTypes
} // namespace OpenDDS

namespace DDS {

using namespace OpenDDS::XTypes;

void DynamicTypeSupport::representations_allowed_by_type(DataRepresentationIdSeq& seq)
{
  // TODO: Need to be able to read annotations?
  seq.length(1);
  seq[0] = XCDR2_DATA_REPRESENTATION;
}

size_t DynamicTypeSupport::key_count() const
{
  size_t count = 0;
  const ReturnCode_t rc = OpenDDS::XTypes::key_count(type_, count);
  if (rc != RETCODE_OK && log_level >= LogLevel::Error) {
    ACE_ERROR((LM_ERROR, "(%P|%t) ERROR: DynamicTypeSupport::key_count: "
      "could not get correct key count for DynamicType %C: %C\n",
      name(), retcode_to_string(rc)));
  }
  return count;
}

Extensibility DynamicTypeSupport::base_extensibility() const
{
  Extensibility ext = OpenDDS::DCPS::FINAL;
  const ReturnCode_t rc = extensibility(type_, ext);
  if (rc != RETCODE_OK && log_level >= LogLevel::Error) {
    ACE_ERROR((LM_ERROR, "(%P|%t) ERROR: DynamicTypeSupport::base_extensibility: "
      "could not get correct extensibility for DynamicType %C: %C\n",
      name(), retcode_to_string(rc)));
  }
  return ext;
}

Extensibility DynamicTypeSupport::max_extensibility() const
{
  Extensibility ext = OpenDDS::DCPS::FINAL;
  const ReturnCode_t rc = OpenDDS::XTypes::max_extensibility(type_, ext);
  if (rc != RETCODE_OK && log_level >= LogLevel::Error) {
    ACE_ERROR((LM_ERROR, "(%P|%t) ERROR: DynamicTypeSupport::max_extensibility: "
      "could not get correct max extensibility for DynamicType %C: %C\n",
      name(), retcode_to_string(rc)));
  }
  return ext;
}

DataWriter_ptr DynamicTypeSupport::create_datawriter()
{
  return new DynamicDataWriterImpl();
}

DataReader_ptr DynamicTypeSupport::create_datareader()
{
  // TODO
  return 0;
}

#  ifndef OPENDDS_NO_MULTI_TOPIC
DataReader_ptr DynamicTypeSupport::create_multitopic_datareader()
{
  // TODO
  return 0;
}
#  endif

const TypeIdentifier& DynamicTypeSupport::getMinimalTypeIdentifier() const
{
  DynamicTypeImpl* const dti = dynamic_cast<DynamicTypeImpl*>(type_.in());
  return dti->get_minimal_type_identifier();
}

const TypeMap& DynamicTypeSupport::getMinimalTypeMap() const
{
  DynamicTypeImpl* const dti = dynamic_cast<DynamicTypeImpl*>(type_.in());
  return dti->get_minimal_type_map();
}

const TypeIdentifier& DynamicTypeSupport::getCompleteTypeIdentifier() const
{
  DynamicTypeImpl* const dti = dynamic_cast<DynamicTypeImpl*>(type_.in());
  return dti->get_complete_type_identifier();
}

const TypeMap& DynamicTypeSupport::getCompleteTypeMap() const
{
  DynamicTypeImpl* const dti = dynamic_cast<DynamicTypeImpl*>(type_.in());
  return dti->get_complete_type_map();
}

DynamicTypeSupport_ptr DynamicTypeSupport::_duplicate(DynamicTypeSupport_ptr obj)
{
  if (obj) {
    obj->_add_ref();
  }
  return obj;
}

} // namespace DDS

OPENDDS_END_VERSIONED_NAMESPACE_DECL

TAO_BEGIN_VERSIONED_NAMESPACE_DECL
namespace TAO {

DDS::DynamicTypeSupport_ptr Objref_Traits<DDS::DynamicTypeSupport>::duplicate(DDS::DynamicTypeSupport_ptr p)
{
  return DDS::DynamicTypeSupport::_duplicate(p);
}

void Objref_Traits<DDS::DynamicTypeSupport>::release(DDS::DynamicTypeSupport_ptr p)
{
  CORBA::release(p);
}

DDS::DynamicTypeSupport_ptr Objref_Traits<DDS::DynamicTypeSupport>::nil()
{
  return static_cast<DDS::DynamicTypeSupport_ptr>(0);
}

CORBA::Boolean Objref_Traits<DDS::DynamicTypeSupport>::marshal(
  const DDS::DynamicTypeSupport_ptr, TAO_OutputCDR&)
{
  return false;
}

} // namespace TAO
TAO_END_VERSIONED_NAMESPACE_DECL

#endif // OPENDDS_SAFETY_PROFILE
