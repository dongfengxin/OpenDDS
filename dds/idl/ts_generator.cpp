/*
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "ts_generator.h"

#include "be_extern.h"
#include "be_util.h"
#include "topic_keys.h"
#include "typeobject_generator.h"

#include <utl_identifier.h>

#include <ace/OS_NS_sys_stat.h>

#include <cstring>
#include <fstream>
#include <sstream>
#include <map>
#include <iostream>

namespace {
  std::string read_template(const char* prefix)
  {
    std::string path = be_util::dds_root();
    path.append("/dds/idl/");
    path.append(prefix);
    path.append("Template.txt");
    std::ifstream ifs(path.c_str());
    if (!ifs) {
      ACE_ERROR((LM_ERROR, "Error - Couldn't open %C\n", path.c_str()));
      return "";
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
  }

  void replaceAll(std::string& s,
                  const std::map<std::string, std::string>& rep) {
    typedef std::map<std::string, std::string>::const_iterator mapiter_t;
    for (size_t i = s.find("<%"); i < s.length(); i = s.find("<%", i + 1)) {
      size_t n = s.find("%>", i) - i + 2;
      mapiter_t iter = rep.find(s.substr(i + 2, n - 4));
      if (iter != rep.end()) {
        s.replace(i, n, iter->second);
      }
    }
  }

  template<size_t N>
  void add_includes(const char* (&includes)[N],
                    BE_GlobalData::stream_enum_t whichStream) {
    for (size_t i = 0; i < N; ++i) {
      be_global->add_include(includes[i], whichStream);
    }
  }
}

ts_generator::ts_generator()
  : idl_template_(read_template("IDL"))
{
}

bool ts_generator::generate_ts(AST_Decl* node, UTL_ScopedName* name)
{
  if (idl_template_.empty()) {
    // error reported in read_template
    return false;
  }

  AST_Structure* struct_node = 0;
  AST_Union* union_node = 0;
  if (!node || !name) {
    return false;
  }
  if (node->node_type() == AST_Decl::NT_struct) {
    struct_node = dynamic_cast<AST_Structure*>(node);
  } else if (node->node_type() == AST_Decl::NT_union) {
    union_node = dynamic_cast<AST_Union*>(node);
  } else {
    return false;
  }

  if (!struct_node && !union_node) {
    idl_global->err()->misc_error(
      "Could not cast AST Nodes to valid types", node);
    return false;
  }

  size_t key_count = 0;
  if (struct_node) {
    IDL_GlobalData::DCPS_Data_Type_Info* info = idl_global->is_dcps_type(name);
    if (be_global->is_topic_type(struct_node)) {
      key_count = TopicKeys(struct_node).count();
    } else if (info) {
      key_count = info->key_list_.size();
    } else {
      return true;
    }
  } else if (be_global->is_topic_type(union_node)) {
    key_count = be_global->union_discriminator_is_key(union_node) ? 1 : 0;
  } else {
    return true;
  }

  const std::string cxx_name = scoped(name);
  const std::string short_name = name->last_component()->get_string();
  const std::string ts_name = scoped(name, EscapeContext_FromGenIdl);
  const std::string ts_short_name = to_string(
    name->last_component(), EscapeContext_FromGenIdl);
  const std::string unescaped_name =
    dds_generator::scoped_helper(name, "::", EscapeContext_StripEscapes);
  const std::string name_underscores = dds_generator::scoped_helper(name, "_");
  static const std::string ns("OpenDDS::DCPS::");
  const std::string xtag = ns + get_xtag_name(name);

  static const char* idl_includes[] = {
    "dds/DdsDcpsInfrastructure.idl", "dds/DdsDcpsTopic.idl",
    "dds/DdsDcpsPublication.idl", "dds/DdsDcpsSubscriptionExt.idl",
    "dds/DdsDcpsTypeSupportExt.idl"
  };
  add_includes(idl_includes, BE_GlobalData::STREAM_IDL);

  std::string dc = be_global->header_name_.c_str();
  dc.replace(dc.end() - 6, dc.end() - 2, "C"); // s/Impl.h$/C.h/
  be_global->add_include(dc.c_str());

  static const char* h_includes[] = {
    "dds/DCPS/TypeSupportImpl.h", "dds/DCPS/ValueDispatcher.h"
  };
  add_includes(h_includes, BE_GlobalData::STREAM_H);

  static const char* cpp_includes[] = {
    "dds/DCPS/debug.h", "dds/DCPS/Registered_Data_Types.h",
    "dds/DdsDcpsDomainC.h", "dds/DCPS/Service_Participant.h",
    "dds/DCPS/Qos_Helper.h", "dds/DCPS/PublicationInstance.h",
    "dds/DCPS/PublisherImpl.h", "dds/DCPS/SubscriberImpl.h",
    "dds/DCPS/ReceivedDataElementList.h", "dds/DCPS/RakeResults_T.h",
    "dds/DCPS/BuiltInTopicUtils.h", "dds/DCPS/Util.h",
    "dds/DCPS/ContentFilteredTopicImpl.h", "dds/DCPS/RakeData.h",
    "dds/DCPS/MultiTopicDataReader_T.h", "dds/DCPS/DataWriterImpl_T.h",
    "dds/DCPS/DataReaderImpl_T.h", "dds/DCPS/XTypes/TypeObject.h"
  };
  add_includes(cpp_includes, BE_GlobalData::STREAM_CPP);

  std::map<std::string, std::string> replacements;
  replacements["SCOPED"] = scoped(name, EscapeContext_ForGenIdl);
  // SCOPED_NOT_GLOBAL is EscapeContext_FromGenIdl, because
  // DCPS_DATA_SEQUENCE_TYPE is strange.
  replacements["SCOPED_NOT_GLOBAL"] =
    dds_generator::scoped_helper(name, "::", EscapeContext_FromGenIdl);
  replacements["TYPE"] = to_string(name->last_component(), EscapeContext_ForGenIdl);
  replacements["EXPORT"] = be_global->export_macro().c_str();
  replacements["SEQ"] = be_global->sequence_suffix().c_str();

  ScopedNamespaceGuard idlGuard(name, be_global->idl_, "module");
  std::string idl = idl_template_;
  replaceAll(idl, replacements);
  be_global->idl_ << idl;

  be_global->header_ << be_global->versioning_begin() << "\n";
  {
    ScopedNamespaceGuard hGuard(name, be_global->header_);

    be_global->header_ <<
      "class " << ts_short_name << "TypeSupportImpl;\n";
  }
  be_global->header_ << be_global->versioning_end() << "\n";

  be_global->header_ <<
    "OPENDDS_BEGIN_VERSIONED_NAMESPACE_DECL\n"
    "namespace OpenDDS {\n"
    "namespace DCPS {\n"
    "template <>\n"
    "struct DDSTraits<" << cxx_name << "> {\n"
    "  typedef " << cxx_name << " MessageType;\n"
    "  typedef " << ts_name << be_global->sequence_suffix() << " MessageSequenceType;\n"
    "  typedef " << ts_name << "TypeSupport TypeSupportType;\n"
    "  typedef " << ts_name << "TypeSupportImpl TypeSupportImplType;\n"
    "  typedef " << ts_name << "DataWriter DataWriterType;\n"
    "  typedef " << ts_name << "DataReader DataReaderType;\n"
    "  typedef " << cxx_name << "_OpenDDS_KeyLessThan LessThanType;\n"
    "  typedef OpenDDS::DCPS::KeyOnly<const " << cxx_name << "> KeyOnlyType;\n"
    "  typedef " << xtag << " XtagType;\n"
    "\n"
    "  static const char* type_name() { return \"" << unescaped_name << "\"; }\n"
    "  static bool gen_has_key() { return " << (key_count ? "true" : "false") << "; }\n"
    "  static size_t key_count() { return " << key_count << "; }\n"
    "};\n"
    "} // namespace DCPS\n"
    "} // namespace OpenDDS\n"
    "OPENDDS_END_VERSIONED_NAMESPACE_DECL\n\n";

  be_global->header_ << be_global->versioning_begin() << "\n";
  {
    ScopedNamespaceGuard hGuard(name, be_global->header_);

    be_global->header_ <<
      "class " << be_global->export_macro() << " " << ts_short_name << "TypeSupportImpl\n"
      "  : public virtual OpenDDS::DCPS::LocalObject<" << ts_short_name << "TypeSupport>\n"
      "  , public virtual OpenDDS::DCPS::TypeSupportImpl_T<" << short_name << ">\n"
      "  , public virtual OpenDDS::DCPS::ValueDispatcher_T<" << short_name << ">\n"
      "{\n"
      "public:\n"
      "  typedef " << ts_short_name << "TypeSupport TypeSupportType;\n"
      "  typedef " << ts_short_name << "TypeSupport::_var_type _var_type;\n"
      "  typedef " << ts_short_name << "TypeSupport::_ptr_type _ptr_type;\n"
      "\n"
      "  " << ts_short_name << "TypeSupportImpl() {}\n"
      "  virtual ~" << ts_short_name << "TypeSupportImpl() {}\n"
      "\n"
      "  virtual " << be_global->versioning_name() << "::DDS::DataWriter_ptr create_datawriter();\n"
      "  virtual " << be_global->versioning_name() << "::DDS::DataReader_ptr create_datareader();\n"
      "#ifndef OPENDDS_NO_MULTI_TOPIC\n"
      "  virtual " << be_global->versioning_name() << "::DDS::DataReader_ptr create_multitopic_datareader();\n"
      "#endif /* !OPENDDS_NO_MULTI_TOPIC */\n"
      "#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE\n"
      "  virtual const OpenDDS::DCPS::MetaStruct& getMetaStructForType();\n"
      "#endif /* !OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE */\n"
      "\n"
      "  virtual const OpenDDS::XTypes::TypeIdentifier& getMinimalTypeIdentifier() const;\n"
      "  virtual const OpenDDS::XTypes::TypeMap& getMinimalTypeMap() const;\n"
      "\n"
      "  virtual const OpenDDS::XTypes::TypeIdentifier& getCompleteTypeIdentifier() const;\n"
      "  virtual const OpenDDS::XTypes::TypeMap& getCompleteTypeMap() const;\n"
      "\n"
      "  static " << ts_short_name << "TypeSupport::_ptr_type _narrow(CORBA::Object_ptr obj);\n"
      "};\n\n";
  }
  be_global->header_ << be_global->versioning_end() << "\n";

  be_global->impl_ << be_global->versioning_begin() << "\n";
  {
    ScopedNamespaceGuard cppGuard(name, be_global->impl_);
    be_global->impl_ <<
      "::DDS::DataWriter_ptr " << ts_short_name << "TypeSupportImpl::create_datawriter()\n"
      "{\n"
      "  typedef OpenDDS::DCPS::DataWriterImpl_T<" << short_name << "> DataWriterImplType;\n"
      "  ::DDS::DataWriter_ptr writer_impl = ::DDS::DataWriter::_nil();\n"
      "  ACE_NEW_NORETURN(writer_impl,\n"
      "                   DataWriterImplType());\n"
      "  return writer_impl;\n"
      "}\n\n"
      "::DDS::DataReader_ptr " << ts_short_name << "TypeSupportImpl::create_datareader()\n"
      "{\n"
      "  typedef OpenDDS::DCPS::DataReaderImpl_T<" << short_name << "> DataReaderImplType;\n"
      "  ::DDS::DataReader_ptr reader_impl = ::DDS::DataReader::_nil();\n"
      "  ACE_NEW_NORETURN(reader_impl,\n"
      "                   DataReaderImplType());\n"
      "  return reader_impl;\n"
      "}\n\n"
      "#ifndef OPENDDS_NO_MULTI_TOPIC\n"
      "::DDS::DataReader_ptr " << ts_short_name << "TypeSupportImpl::create_multitopic_datareader()\n"
      "{\n"
      "  typedef OpenDDS::DCPS::DataReaderImpl_T<" << short_name << "> DataReaderImplType;\n"
      "  typedef OpenDDS::DCPS::MultiTopicDataReader_T<" << short_name << ", DataReaderImplType> MultiTopicDataReaderImplType;\n"
      "  ::DDS::DataReader_ptr multitopic_reader_impl = ::DDS::DataReader::_nil();\n"
      "  ACE_NEW_NORETURN(multitopic_reader_impl,\n"
      "                   MultiTopicDataReaderImplType());\n"
      "  return multitopic_reader_impl;\n"
      "}\n"
      "#endif /* !OPENDDS_NO_MULTI_TOPIC */\n\n"
      "#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE\n"
      "const OpenDDS::DCPS::MetaStruct& " << ts_short_name << "TypeSupportImpl::getMetaStructForType()\n"
      "{\n"
      "  return OpenDDS::DCPS::getMetaStruct<" << short_name << ">();\n"
      "}\n"
      "#endif /* !OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE */\n\n"
      "namespace {\n"
      "  OpenDDS::DCPS::TypeSupportInitializer<" << ts_short_name << "TypeSupportImpl> ts_init_" << name_underscores << ";\n"
      "}\n"
      "\n"
      "const OpenDDS::XTypes::TypeIdentifier& " << ts_short_name << "TypeSupportImpl::getMinimalTypeIdentifier() const\n"
      "{\n";

    const bool java_ts_only = be_global->java_arg().length() > 0;
    const bool generate_xtypes = !be_global->suppress_xtypes() && !java_ts_only;
    if (generate_xtypes) {
      be_global->impl_ <<
        "  return OpenDDS::DCPS::getMinimalTypeIdentifier<" << xtag << ">();\n";
    } else {
      be_global->impl_ <<
        "  static OpenDDS::XTypes::TypeIdentifier ti;\n"
        "  return ti;\n";
    }
    be_global->impl_ <<
      "}\n\n"
      "const OpenDDS::XTypes::TypeMap& " << ts_short_name << "TypeSupportImpl::getMinimalTypeMap() const\n"
      "{\n";

    if (generate_xtypes) {
      be_global->impl_ <<
        "  return OpenDDS::DCPS::getMinimalTypeMap<" << xtag << ">();\n";
    } else {
      be_global->impl_ <<
        "  static OpenDDS::XTypes::TypeMap tm;\n"
        "  return tm;\n";
    }
    be_global->impl_ <<
      "}\n\n"
      "const OpenDDS::XTypes::TypeIdentifier& " << ts_short_name << "TypeSupportImpl::getCompleteTypeIdentifier() const\n"
      "{\n";

    const bool generate_xtypes_complete = generate_xtypes && be_global->xtypes_complete();
    if (generate_xtypes_complete) {
      be_global->impl_ <<
        "  return OpenDDS::DCPS::getCompleteTypeIdentifier<" << xtag << ">();\n";
    } else {
      be_global->impl_ <<
        "  static OpenDDS::XTypes::TypeIdentifier ti;\n"
        "  return ti;\n";
    }
    be_global->impl_ <<
      "}\n\n"
      "const OpenDDS::XTypes::TypeMap& " << ts_short_name << "TypeSupportImpl::getCompleteTypeMap() const\n"
      "{\n";

    if (generate_xtypes_complete) {
      be_global->impl_ <<
        "  return OpenDDS::DCPS::getCompleteTypeMap<" << xtag << ">();\n";
    } else {
      be_global->impl_ <<
        "  static OpenDDS::XTypes::TypeMap tm;\n"
        "  return tm;\n";
    }
    be_global->impl_ <<
      "}\n\n"
      << ts_short_name << "TypeSupport::_ptr_type " << ts_short_name << "TypeSupportImpl::_narrow(CORBA::Object_ptr obj)\n"
      "{\n"
      "  return TypeSupportType::_narrow(obj);\n"
      "}\n";
  }
  be_global->impl_ << be_global->versioning_end() << "\n";

  if (be_global->face_ts()) {
    if (node->node_type() == AST_Decl::NT_struct) {
      face_ts_generator::generate(name);
    } else {
      idl_global->err()->misc_error(
        "Generating FACE type support for Union topic types is not supported", node);
      return false;
    }
  }

  return true;
}

bool ts_generator::gen_struct(AST_Structure* node, UTL_ScopedName* name,
  const std::vector<AST_Field*>&, AST_Type::SIZE_TYPE, const char*)
{
  return generate_ts(node, name);
}

bool ts_generator::gen_union(AST_Union* node, UTL_ScopedName* name,
  const std::vector<AST_UnionBranch*>&, AST_Type*, const char*)
{
  return generate_ts(node, name);
}

namespace java_ts_generator {

  /// called directly by dds_visitor::visit_structure() if -Wb,java
  void generate(AST_Structure* node) {
    UTL_ScopedName* name = node->name();

    if (!(idl_global->is_dcps_type(name) || be_global->is_topic_type(node))) {
      return;
    }

    ACE_CString output_file = be_global->java_arg();
    if (output_file.length()) {
      be_global->impl_name_ = output_file;
    }
    be_global->add_include("idl2jni_jni.h", BE_GlobalData::STREAM_CPP);

    std::string type = scoped(name);

    std::string file, jniclass, jpackage;
    for (UTL_ScopedName* sn = name; sn;
        sn = static_cast<UTL_ScopedName*>(sn->tail())) {
      std::string tmp = sn->head()->get_string();
      if (!tmp.empty() && sn->tail()) {
        jpackage += tmp;
        file += tmp;
        if (ACE_OS::mkdir(file.c_str()) != 0 && errno != EEXIST) {
          ACE_ERROR((LM_ERROR, ACE_TEXT("ERROR: java_ts_generator::generate - ")
            ACE_TEXT("unable to create specified directory: %C"), file.c_str()));
        }
      }
      for (size_t i = tmp.find('_'); i < tmp.length();
          i = tmp.find('_', i + 1)) {
        tmp.insert(++i, 1, '1');
      }
      jniclass += tmp;
      if (!jniclass.empty() && sn->tail()) {
        jniclass += '_';
        jpackage += '.';
        file += '/';
      }
    }

    if (jpackage.size() && jpackage[jpackage.size() - 1] == '.') {
      jpackage.resize(jpackage.size() - 1);
    }

    std::string clazz = name->last_component()->get_string();
    file += clazz + "TypeSupportImpl.java";

    std::ofstream java(file.c_str());
    java << (jpackage.size() ? "package " : "") << jpackage
      << (jpackage.size() ? ";\n" :"") <<
      "public class " << clazz << "TypeSupportImpl extends _" << clazz
      << "TypeSupportTAOPeer {\n"
      "    public " << clazz << "TypeSupportImpl() {\n"
      "        super(_jni_init());\n"
      "    }\n"
      "    private static native long _jni_init();\n"
      "}\n";
    be_global->impl_ <<
      "extern \"C\" JNIEXPORT jlong JNICALL\n"
      "Java_" << jniclass << "TypeSupportImpl__1jni_1init(JNIEnv*, jclass) {\n"
      "  return reinterpret_cast<jlong>(static_cast<CORBA::Object_ptr>(new "
      << type << "TypeSupportImpl));\n"
      "}\n\n";
  }

}

namespace face_ts_generator {

  void generate(UTL_ScopedName* name) {
    const std::string cxx_name = scoped(name),
      name_underscores = dds_generator::scoped_helper(name, "_"),
      exportMacro = be_global->export_macro().c_str(),
      exporter = exportMacro.empty() ? "" : ("    " + exportMacro + '\n');
    be_global->add_include("FACE/TS.hpp", BE_GlobalData::STREAM_FACETS_H);
    be_global->facets_header_ <<
      "namespace FACE\n"
      "{\n"
      "  namespace Read_Callback\n"
      "  {\n"
      "    typedef void (*send_event_" << name_underscores << "_Ptr) (\n"
      "      /* in */ TRANSACTION_ID_TYPE transaction_id,\n"
      "      /* inout */ " << cxx_name << "& message,\n"
      "      /* in */ MESSAGE_TYPE_GUID message_type_id,\n"
      "      /* in */ MESSAGE_SIZE_TYPE message_size,\n"
      "      /* in */ const WAITSET_TYPE waitset,\n"
      "      /* out */ RETURN_CODE_TYPE& return_code);\n"
      "  }\n\n"
      "  namespace TS\n"
      "  {\n" << exporter <<
      "    void Receive_Message(\n"
      "      /* in */ CONNECTION_ID_TYPE connection_id,\n"
      "      /* in */ TIMEOUT_TYPE timeout,\n"
      "      /* inout */ TRANSACTION_ID_TYPE& transaction_id,\n"
      "      /* out */ " << cxx_name << "& message,\n"
      "      /* in */ MESSAGE_SIZE_TYPE message_size,\n"
      "      /* out */ RETURN_CODE_TYPE& return_code);\n\n" << exporter <<
      "    void Send_Message(\n"
      "      /* in */ CONNECTION_ID_TYPE connection_id,\n"
      "      /* in */ TIMEOUT_TYPE timeout,\n"
      "      /* inout */ TRANSACTION_ID_TYPE& transaction_id,\n"
      "      /* inout */ " << cxx_name << "& message,\n"
      "      /* inout */ MESSAGE_SIZE_TYPE& message_size,\n"
      "      /* out */ RETURN_CODE_TYPE& return_code);\n\n" << exporter <<
      "    void Register_Callback(\n"
      "      /* in */ CONNECTION_ID_TYPE connection_id,\n"
      "      /* in */ const WAITSET_TYPE waitset,\n"
      "      /* in */ Read_Callback::send_event_" << name_underscores
                    << "_Ptr data_callback,\n"
      "      /* in */ MESSAGE_SIZE_TYPE max_message_size,\n"
      "      /* out */ RETURN_CODE_TYPE& return_code);\n\n"
      "  }\n"
      "}\n\n";
    be_global->facets_impl_ <<
      "void Receive_Message(CONNECTION_ID_TYPE connection_id,\n"
      "                     TIMEOUT_TYPE timeout,\n"
      "                     TRANSACTION_ID_TYPE& transaction_id,\n"
      "                     " << cxx_name << "& message,\n"
      "                     MESSAGE_SIZE_TYPE message_size,\n"
      "                     RETURN_CODE_TYPE& return_code) {\n"
      "  OpenDDS::FaceTSS::receive_message(connection_id, timeout,\n"
      "                                    transaction_id, message,\n"
      "                                    message_size, return_code);\n"
      "}\n\n"
      "void Send_Message(CONNECTION_ID_TYPE connection_id,\n"
      "                  TIMEOUT_TYPE timeout,\n"
      "                  TRANSACTION_ID_TYPE& transaction_id,\n"
      "                  " << cxx_name << "& message,\n"
      "                  MESSAGE_SIZE_TYPE& message_size,\n"
      "                  RETURN_CODE_TYPE& return_code) {\n"
      "  OpenDDS::FaceTSS::send_message(connection_id, timeout,\n"
      "                                 transaction_id, message,\n"
      "                                 message_size, return_code);\n"
      "}\n\n"
      "void Register_Callback(CONNECTION_ID_TYPE connection_id,\n"
      "                       const WAITSET_TYPE waitset,\n"
      "                       Read_Callback::send_event_" << name_underscores
                          << "_Ptr data_callback,\n"
      "                       MESSAGE_SIZE_TYPE max_message_size,\n"
      "                       RETURN_CODE_TYPE& return_code) {\n"
      "  OpenDDS::FaceTSS::register_callback(connection_id, waitset,\n"
      "                                      data_callback,\n"
      "                                      max_message_size, return_code);\n"
      "}\n\n"
      "namespace {\n"
      "  OpenDDS::DCPS::TypeSupportInitializer<" << cxx_name << "TypeSupportImpl> ts_init_" << name_underscores << ";\n"
      "}\n\n";
  }
}
