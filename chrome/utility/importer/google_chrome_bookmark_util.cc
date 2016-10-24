#include "chrome/utility/importer/google_chrome_bookmark_util.h"

#include "base/json/json_string_value_serializer.h"
#include "base/pickle.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
//#include "components/webdata/encryptor/encryptor.h"
#include "components/os_crypt/os_crypt.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "grit/generated_resources.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "ui/base/l10n/l10n_util.h"
#include "grit/components_strings.h"

using base::Time;

namespace {

// Helper to recursively determine if a Dictionary has any valid values.
bool HasValues(const base::DictionaryValue& root) {
  if (root.empty())
    return false;
  for (base::DictionaryValue::Iterator iter(root); !iter.IsAtEnd();
       iter.Advance()) {
    const base::Value& value = iter.value();
    if (value.IsType(base::Value::TYPE_DICTIONARY)) {
      const base::DictionaryValue* dict_value = NULL;
      if (value.GetAsDictionary(&dict_value) && HasValues(*dict_value))
        return true;
    } else {
      // A non dictionary type was encountered, assume it's a valid value.
      return true;
    }
  }
  return false;
}

// Whitespace characters to strip from bookmark titles.
const char16 kInvalidChars[] = {
  '\n', '\r', '\t',
  0x2028,  // Line separator
  0x2029,  // Paragraph separator
  0
};

}  // namespace

namespace importer {

// BookmarkNode ---------------------------------------------------------------

  const int64_t BookmarkNode::kInvalidSyncTransactionVersion = -1;

  BookmarkNode::BookmarkNode(const GURL& url)
    : url_(url) {
    Initialize(0);
  }

  BookmarkNode::BookmarkNode(int64_t id, const GURL& url) : url_(url) {
    Initialize(id);
  }

  BookmarkNode::~BookmarkNode() {
  }

  void BookmarkNode::SetTitle(const base::string16& title) {
    // Replace newlines and other problematic whitespace characters in
    // folder/bookmark names with spaces.
    base::string16 trimmed_title;
    base::ReplaceChars(title, kInvalidChars, base::ASCIIToUTF16(" "),
      &trimmed_title);
    ui::TreeNode<BookmarkNode>::SetTitle(trimmed_title);
  }

  bool BookmarkNode::IsVisible() const {
    return true;
  }

  bool BookmarkNode::GetMetaInfo(const std::string& key,
    std::string* value) const {
    if (!meta_info_map_)
      return false;

    MetaInfoMap::const_iterator it = meta_info_map_->find(key);
    if (it == meta_info_map_->end())
      return false;

    *value = it->second;
    return true;
  }

  bool BookmarkNode::SetMetaInfo(const std::string& key,
    const std::string& value) {
    if (!meta_info_map_)
      meta_info_map_.reset(new MetaInfoMap);

    MetaInfoMap::iterator it = meta_info_map_->find(key);
    if (it == meta_info_map_->end()) {
      (*meta_info_map_)[key] = value;
      return true;
    }
    // Key already in map, check if the value has changed.
    if (it->second == value)
      return false;
    it->second = value;
    return true;
  }

  bool BookmarkNode::DeleteMetaInfo(const std::string& key) {
    if (!meta_info_map_)
      return false;
    bool erased = meta_info_map_->erase(key) != 0;
    if (meta_info_map_->empty())
      meta_info_map_.reset();
    return erased;
  }

  void BookmarkNode::SetMetaInfoMap(const MetaInfoMap& meta_info_map) {
    if (meta_info_map.empty())
      meta_info_map_.reset();
    else
      meta_info_map_.reset(new MetaInfoMap(meta_info_map));
  }

  const BookmarkNode::MetaInfoMap* BookmarkNode::GetMetaInfoMap() const {
    return meta_info_map_.get();
  }

  void BookmarkNode::Initialize(int64_t id) {
    id_ = id;
    type_ = url_.is_empty() ? FOLDER : URL;
    date_added_ = base::Time::Now();
    favicon_type_ = favicon_base::INVALID_ICON;
    favicon_state_ = INVALID_FAVICON;
    favicon_load_task_id_ = base::CancelableTaskTracker::kBadTaskId;
    meta_info_map_.reset();
    sync_transaction_version_ = kInvalidSyncTransactionVersion;
  }

  void BookmarkNode::InvalidateFavicon() {
    icon_url_ = GURL();
    favicon_ = gfx::Image();
    favicon_type_ = favicon_base::INVALID_ICON;
    favicon_state_ = INVALID_FAVICON;
  }

// -----------------------------------------------------------------------------
// BookmarkCodec

const char BookmarkCodec::kRootsKey[] = "roots";
const char BookmarkCodec::kRootFolderNameKey[] = "bookmark_bar";
const char BookmarkCodec::kOtherBookmarkFolderNameKey[] = "other";
// The value is left as 'synced' for historical reasons.
const char BookmarkCodec::kMobileBookmarkFolderNameKey[] = "synced";
const char BookmarkCodec::kVersionKey[] = "version";
const char BookmarkCodec::kChecksumKey[] = "checksum";
const char BookmarkCodec::kIdKey[] = "id";
const char BookmarkCodec::kTypeKey[] = "type";
const char BookmarkCodec::kNameKey[] = "name";
const char BookmarkCodec::kDateAddedKey[] = "date_added";
const char BookmarkCodec::kURLKey[] = "url";
const char BookmarkCodec::kDateModifiedKey[] = "date_modified";
const char BookmarkCodec::kChildrenKey[] = "children";
const char BookmarkCodec::kMetaInfo[] = "meta_info";
const char BookmarkCodec::kSyncTransactionVersion[] =
"sync_transaction_version";
const char BookmarkCodec::kTypeURL[] = "url";
const char BookmarkCodec::kTypeFolder[] = "folder";

// Current version of the file.
static const int kCurrentVersion = 1;

BookmarkCodec::BookmarkCodec()
  : ids_reassigned_(false),
  ids_valid_(true),
  maximum_id_(0),
  model_sync_transaction_version_(
    BookmarkNode::kInvalidSyncTransactionVersion) {
}

BookmarkCodec::~BookmarkCodec() {}


bool BookmarkCodec::Decode(BookmarkNode* bb_node,
  BookmarkNode* other_folder_node,
  BookmarkNode* mobile_folder_node,
  int64_t* max_id,
  const base::Value& value) {
  ids_.clear();
  ids_reassigned_ = false;
  ids_valid_ = true;
  maximum_id_ = 0;
  stored_checksum_.clear();
  InitializeChecksum();
  bool success = DecodeHelper(bb_node, other_folder_node, mobile_folder_node,
    value);
  FinalizeChecksum();
  // If either the checksums differ or some IDs were missing/not unique,
  // reassign IDs.
  if (!ids_valid_ || computed_checksum() != stored_checksum())
    ReassignIDs(bb_node, other_folder_node, mobile_folder_node);
  *max_id = maximum_id_ + 1;
  return success;
}

std::unique_ptr<base::Value> BookmarkCodec::EncodeNode(
  const BookmarkNode* node) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  std::string id = base::Int64ToString(node->id());
  value->SetString(kIdKey, id);
  const base::string16& title = node->GetTitle();
  value->SetString(kNameKey, title);
  value->SetString(kDateAddedKey,
    base::Int64ToString(node->date_added().ToInternalValue()));
  if (node->is_url()) {
    value->SetString(kTypeKey, kTypeURL);
    std::string url = node->url().possibly_invalid_spec();
    value->SetString(kURLKey, url);
    UpdateChecksumWithUrlNode(id, title, url);
  }
  else {
    value->SetString(kTypeKey, kTypeFolder);
    value->SetString(
      kDateModifiedKey,
      base::Int64ToString(node->date_folder_modified().ToInternalValue()));
    UpdateChecksumWithFolderNode(id, title);

    base::ListValue* child_values = new base::ListValue();
    value->Set(kChildrenKey, child_values);
    for (int i = 0; i < node->child_count(); ++i)
      child_values->Append(EncodeNode(node->GetChild(i)));
  }
  const BookmarkNode::MetaInfoMap* meta_info_map = node->GetMetaInfoMap();
  if (meta_info_map)
    value->Set(kMetaInfo, EncodeMetaInfo(*meta_info_map));
  if (node->sync_transaction_version() !=
    BookmarkNode::kInvalidSyncTransactionVersion) {
    value->SetString(kSyncTransactionVersion,
      base::Int64ToString(node->sync_transaction_version()));
  }
  return std::move(value);
}

base::Value* BookmarkCodec::EncodeMetaInfo(
  const BookmarkNode::MetaInfoMap& meta_info_map) {
  base::DictionaryValue* meta_info = new base::DictionaryValue;
  for (BookmarkNode::MetaInfoMap::const_iterator it = meta_info_map.begin();
    it != meta_info_map.end(); ++it) {
    meta_info->SetStringWithoutPathExpansion(it->first, it->second);
  }
  return meta_info;
}

bool BookmarkCodec::DecodeHelper(BookmarkNode* bb_node,
  BookmarkNode* other_folder_node,
  BookmarkNode* mobile_folder_node,
  const base::Value& value) {
  const base::DictionaryValue* d_value = nullptr;
  if (!value.GetAsDictionary(&d_value))
    return false;  // Unexpected type.

  int version;
  if (!d_value->GetInteger(kVersionKey, &version) || version != kCurrentVersion)
    return false;  // Unknown version.

  const base::Value* checksum_value;
  if (d_value->Get(kChecksumKey, &checksum_value)) {
    if (checksum_value->GetType() != base::Value::TYPE_STRING)
      return false;
    if (!checksum_value->GetAsString(&stored_checksum_))
      return false;
  }

  const base::Value* roots;
  if (!d_value->Get(kRootsKey, &roots))
    return false;  // No roots.

  const base::DictionaryValue* roots_d_value = nullptr;
  if (!roots->GetAsDictionary(&roots_d_value))
    return false;  // Invalid type for roots.
  const base::Value* root_folder_value;
  const base::Value* other_folder_value = nullptr;
  const base::DictionaryValue* root_folder_d_value = nullptr;
  const base::DictionaryValue* other_folder_d_value = nullptr;
  if (!roots_d_value->Get(kRootFolderNameKey, &root_folder_value) ||
    !root_folder_value->GetAsDictionary(&root_folder_d_value) ||
    !roots_d_value->Get(kOtherBookmarkFolderNameKey, &other_folder_value) ||
    !other_folder_value->GetAsDictionary(&other_folder_d_value)) {
    return false;  // Invalid type for root folder and/or other
                   // folder.
  }
  DecodeNode(*root_folder_d_value, nullptr, bb_node);
  DecodeNode(*other_folder_d_value, nullptr, other_folder_node);

  // Fail silently if we can't deserialize mobile bookmarks. We can't require
  // them to exist in order to be backwards-compatible with older versions of
  // chrome.
  const base::Value* mobile_folder_value;
  const base::DictionaryValue* mobile_folder_d_value = nullptr;
  if (roots_d_value->Get(kMobileBookmarkFolderNameKey, &mobile_folder_value) &&
    mobile_folder_value->GetAsDictionary(&mobile_folder_d_value)) {
    DecodeNode(*mobile_folder_d_value, nullptr, mobile_folder_node);
  }
  else {
    // If we didn't find the mobile folder, we're almost guaranteed to have a
    // duplicate id when we add the mobile folder. Consequently, if we don't
    // intend to reassign ids in the future (ids_valid_ is still true), then at
    // least reassign the mobile bookmarks to avoid it colliding with anything
    // else.
    if (ids_valid_)
      ReassignIDsHelper(mobile_folder_node);
  }

  if (!DecodeMetaInfo(*roots_d_value, &model_meta_info_map_,
    &model_sync_transaction_version_))
    return false;

  std::string sync_transaction_version_str;
  if (roots_d_value->GetString(kSyncTransactionVersion,
    &sync_transaction_version_str) &&
    !base::StringToInt64(sync_transaction_version_str,
      &model_sync_transaction_version_))
    return false;

  // Need to reset the type as decoding resets the type to FOLDER. Similarly
  // we need to reset the title as the title is persisted and restored from
  // the file.
  bb_node->set_type(BookmarkNode::BOOKMARK_BAR);
  other_folder_node->set_type(BookmarkNode::OTHER_NODE);
  mobile_folder_node->set_type(BookmarkNode::MOBILE);
  bb_node->SetTitle(l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_FOLDER_NAME));
  other_folder_node->SetTitle(
    l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OTHER_FOLDER_NAME));
  mobile_folder_node->SetTitle(
    l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_MOBILE_FOLDER_NAME));

  return true;
}

bool BookmarkCodec::DecodeChildren(const base::ListValue& child_value_list,
  BookmarkNode* parent) {
  for (size_t i = 0; i < child_value_list.GetSize(); ++i) {
    const base::Value* child_value;
    if (!child_value_list.Get(i, &child_value))
      return false;

    const base::DictionaryValue* child_d_value = nullptr;
    if (!child_value->GetAsDictionary(&child_d_value))
      return false;
    DecodeNode(*child_d_value, parent, nullptr);
  }
  return true;
}

bool BookmarkCodec::DecodeNode(const base::DictionaryValue& value,
  BookmarkNode* parent,
  BookmarkNode* node) {
  // If no |node| is specified, we'll create one and add it to the |parent|.
  // Therefore, in that case, |parent| must be non-NULL.
  if (!node && !parent) {
    NOTREACHED();
    return false;
  }

  // It's not valid to have both a node and a specified parent.
  if (node && parent) {
    NOTREACHED();
    return false;
  }

  std::string id_string;
  int64_t id = 0;
  if (ids_valid_) {
    if (!value.GetString(kIdKey, &id_string) ||
      !base::StringToInt64(id_string, &id) ||
      ids_.count(id) != 0) {
      ids_valid_ = false;
    }
    else {
      ids_.insert(id);
    }
  }

  maximum_id_ = std::max(maximum_id_, id);

  base::string16 title;
  value.GetString(kNameKey, &title);

  std::string date_added_string;
  if (!value.GetString(kDateAddedKey, &date_added_string))
    date_added_string = base::Int64ToString(Time::Now().ToInternalValue());
  int64_t internal_time;
  base::StringToInt64(date_added_string, &internal_time);

  std::string type_string;
  if (!value.GetString(kTypeKey, &type_string))
    return false;

  if (type_string != kTypeURL && type_string != kTypeFolder)
    return false;  // Unknown type.

  if (type_string == kTypeURL) {
    std::string url_string;
    if (!value.GetString(kURLKey, &url_string))
      return false;

    GURL url = GURL(url_string);
    if (!node && url.is_valid())
      node = new BookmarkNode(id, url);
    else
      return false;  // Node invalid.

    if (parent)
      parent->Add(base::WrapUnique(node), parent->child_count());
    node->set_type(BookmarkNode::URL);
    UpdateChecksumWithUrlNode(id_string, title, url_string);
  }
  else {
    std::string last_modified_date;
    if (!value.GetString(kDateModifiedKey, &last_modified_date))
      last_modified_date = base::Int64ToString(Time::Now().ToInternalValue());

    const base::Value* child_values;
    if (!value.Get(kChildrenKey, &child_values))
      return false;

    if (child_values->GetType() != base::Value::TYPE_LIST)
      return false;

    if (!node) {
      node = new BookmarkNode(id, GURL());
    }
    else {
      // If a new node is not created, explicitly assign ID to the existing one.
      node->set_id(id);
    }

    node->set_type(BookmarkNode::FOLDER);
    int64_t internal_time;
    base::StringToInt64(last_modified_date, &internal_time);
    node->set_date_folder_modified(Time::FromInternalValue(internal_time));

    if (parent)
      parent->Add(base::WrapUnique(node), parent->child_count());

    UpdateChecksumWithFolderNode(id_string, title);

    const base::ListValue* child_l_values = nullptr;
    if (!child_values->GetAsList(&child_l_values))
      return false;
    if (!DecodeChildren(*child_l_values, node))
      return false;
  }

  node->SetTitle(title);
  node->set_date_added(Time::FromInternalValue(internal_time));

  int64_t sync_transaction_version = node->sync_transaction_version();
  BookmarkNode::MetaInfoMap meta_info_map;
  if (!DecodeMetaInfo(value, &meta_info_map, &sync_transaction_version))
    return false;
  node->SetMetaInfoMap(meta_info_map);

  std::string sync_transaction_version_str;
  if (value.GetString(kSyncTransactionVersion, &sync_transaction_version_str) &&
    !base::StringToInt64(sync_transaction_version_str,
      &sync_transaction_version))
    return false;

  node->set_sync_transaction_version(sync_transaction_version);

  return true;
}

bool BookmarkCodec::DecodeMetaInfo(const base::DictionaryValue& value,
  BookmarkNode::MetaInfoMap* meta_info_map,
  int64_t* sync_transaction_version) {
  DCHECK(meta_info_map);
  DCHECK(sync_transaction_version);
  meta_info_map->clear();

  const base::Value* meta_info;
  if (!value.Get(kMetaInfo, &meta_info))
    return true;

  std::unique_ptr<base::Value> deserialized_holder;

  // Meta info used to be stored as a serialized dictionary, so attempt to
  // parse the value as one.
  if (meta_info->IsType(base::Value::TYPE_STRING)) {
    std::string meta_info_str;
    meta_info->GetAsString(&meta_info_str);
    JSONStringValueDeserializer deserializer(meta_info_str);
    deserialized_holder = deserializer.Deserialize(nullptr, nullptr);
    if (!deserialized_holder)
      return false;
    meta_info = deserialized_holder.get();
  }
  // meta_info is now either the kMetaInfo node, or the deserialized node if it
  // was stored as a string. Either way it should now be a (possibly nested)
  // dictionary of meta info values.
  const base::DictionaryValue* meta_info_dict;
  if (!meta_info->GetAsDictionary(&meta_info_dict))
    return false;
  DecodeMetaInfoHelper(*meta_info_dict, std::string(), meta_info_map);

  // Previously sync transaction version was stored in the meta info field
  // using this key. If the key is present when decoding, set the sync
  // transaction version to its value, then delete the field.
  if (deserialized_holder) {
    const char kBookmarkTransactionVersionKey[] = "sync.transaction_version";
    BookmarkNode::MetaInfoMap::iterator it =
      meta_info_map->find(kBookmarkTransactionVersionKey);
    if (it != meta_info_map->end()) {
      base::StringToInt64(it->second, sync_transaction_version);
      meta_info_map->erase(it);
    }
  }

  return true;
}

void BookmarkCodec::DecodeMetaInfoHelper(
  const base::DictionaryValue& dict,
  const std::string& prefix,
  BookmarkNode::MetaInfoMap* meta_info_map) {
  for (base::DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    if (it.value().IsType(base::Value::TYPE_DICTIONARY)) {
      const base::DictionaryValue* subdict;
      it.value().GetAsDictionary(&subdict);
      DecodeMetaInfoHelper(*subdict, prefix + it.key() + ".", meta_info_map);
    }
    else if (it.value().IsType(base::Value::TYPE_STRING)) {
      it.value().GetAsString(&(*meta_info_map)[prefix + it.key()]);
    }
  }
}

void BookmarkCodec::ReassignIDs(BookmarkNode* bb_node,
  BookmarkNode* other_node,
  BookmarkNode* mobile_node) {
  maximum_id_ = 0;
  ReassignIDsHelper(bb_node);
  ReassignIDsHelper(other_node);
  ReassignIDsHelper(mobile_node);
  ids_reassigned_ = true;
}

void BookmarkCodec::ReassignIDsHelper(BookmarkNode* node) {
  DCHECK(node);
  node->set_id(++maximum_id_);
  for (int i = 0; i < node->child_count(); ++i)
    ReassignIDsHelper(node->GetChild(i));
}

void BookmarkCodec::UpdateChecksum(const std::string& str) {
  base::MD5Update(&md5_context_, str);
}

void BookmarkCodec::UpdateChecksum(const base::string16& str) {
  base::MD5Update(&md5_context_,
    base::StringPiece(
      reinterpret_cast<const char*>(str.data()),
      str.length() * sizeof(str[0])));
}

void BookmarkCodec::UpdateChecksumWithUrlNode(const std::string& id,
  const base::string16& title,
  const std::string& url) {
  DCHECK(base::IsStringUTF8(url));
  UpdateChecksum(id);
  UpdateChecksum(title);
  UpdateChecksum(kTypeURL);
  UpdateChecksum(url);
}

void BookmarkCodec::UpdateChecksumWithFolderNode(const std::string& id,
  const base::string16& title) {
  UpdateChecksum(id);
  UpdateChecksum(title);
  UpdateChecksum(kTypeFolder);
}

void BookmarkCodec::InitializeChecksum() {
  base::MD5Init(&md5_context_);
}

void BookmarkCodec::FinalizeChecksum() {
  base::MD5Digest digest;
  base::MD5Final(&digest, &md5_context_);
  computed_checksum_ = base::MD5DigestToBase16(digest);
}


// -----------------------------------------------------------------------------
// LoginDatabase

namespace {
static const int kCurrentVersionNumber = 4;
static const int kCompatibleVersionNumber = 1;

// Convenience enum for interacting with SQL queries that use all the columns.
enum LoginTableColumns {
  COLUMN_ORIGIN_URL = 0,
  COLUMN_ACTION_URL,
  COLUMN_USERNAME_ELEMENT,
  COLUMN_USERNAME_VALUE,
  COLUMN_PASSWORD_ELEMENT,
  COLUMN_PASSWORD_VALUE,
  COLUMN_SUBMIT_ELEMENT,
  COLUMN_SIGNON_REALM,
  COLUMN_SSL_VALID,
  COLUMN_PREFERRED,
  COLUMN_DATE_CREATED,
  COLUMN_BLACKLISTED_BY_USER,
  COLUMN_SCHEME,
  COLUMN_PASSWORD_TYPE,
  COLUMN_POSSIBLE_USERNAMES,
  COLUMN_TIMES_USED,
  //COLUMN_FORM_DATA
};

}

using autofill::PasswordForm;

LoginDatabase::LoginDatabase() {
}

LoginDatabase::~LoginDatabase() {
}

bool LoginDatabase::Init(const base::FilePath& db_path) {
  // Set pragmas for a small, private database (based on WebDatabase).
  db_.set_page_size(2048);
  db_.set_cache_size(32);
  db_.set_exclusive_locking();
  db_.set_restrict_to_user();

  if (!db_.Open(db_path)) {
    LOG(WARNING) << "Unable to open the password store database.";
    return false;
  }

  sql::Transaction transaction(&db_);
  transaction.Begin();

  // Check the database version.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    db_.Close();
    return false;
  }
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Password store database is too new.";
    db_.Close();
    return false;
  }

  // Initialize the tables.
  if (!InitLoginsTable()) {
    LOG(WARNING) << "Unable to initialize the password store database.";
    db_.Close();
    return false;
  }

  // Save the path for DeleteDatabaseFile().
  db_path_ = db_path;

  if (!transaction.Commit()) {
    db_.Close();
    return false;
  }

  return true;
}

bool LoginDatabase::InitLoginsTable() {
  if (!db_.DoesTableExist("logins")) {
    return false;
  }
  return true;
}

bool LoginDatabase::GetAutofillableLogins(
    std::vector<PasswordForm*>* forms) const {
  return GetAllLoginsWithBlacklistSetting(false, forms);
}

bool LoginDatabase::GetBlacklistLogins(
    std::vector<PasswordForm*>* forms) const {
  return GetAllLoginsWithBlacklistSetting(true, forms);
}

LoginDatabase::EncryptionResult LoginDatabase::InitPasswordFormFromStatement(
    PasswordForm* form,
    sql::Statement& s) const {
  std::string encrypted_password;
  s.ColumnBlobAsString(COLUMN_PASSWORD_VALUE, &encrypted_password);
  string16 decrypted_password;
  EncryptionResult encryption_result =
      DecryptedString(encrypted_password, &decrypted_password);
  if (encryption_result != ENCRYPTION_RESULT_SUCCESS)
    return encryption_result;

  std::string tmp = s.ColumnString(COLUMN_ORIGIN_URL);
  form->origin = GURL(tmp);
  tmp = s.ColumnString(COLUMN_ACTION_URL);
  form->action = GURL(tmp);
  form->username_element = s.ColumnString16(COLUMN_USERNAME_ELEMENT);
  form->username_value = s.ColumnString16(COLUMN_USERNAME_VALUE);
  form->password_element = s.ColumnString16(COLUMN_PASSWORD_ELEMENT);
  form->password_value = decrypted_password;
  form->submit_element = s.ColumnString16(COLUMN_SUBMIT_ELEMENT);
  tmp = s.ColumnString(COLUMN_SIGNON_REALM);
  form->signon_realm = tmp;
  //form->ssl_valid = (s.ColumnInt(COLUMN_SSL_VALID) > 0);
  form->preferred = (s.ColumnInt(COLUMN_PREFERRED) > 0);
  form->date_created = base::Time::FromTimeT(
      s.ColumnInt64(COLUMN_DATE_CREATED));
  form->blacklisted_by_user = (s.ColumnInt(COLUMN_BLACKLISTED_BY_USER) > 0);
  int scheme_int = s.ColumnInt(COLUMN_SCHEME);
  DCHECK((scheme_int >= 0) && (scheme_int <= PasswordForm::SCHEME_OTHER));
  form->scheme = static_cast<PasswordForm::Scheme>(scheme_int);
  int type_int = s.ColumnInt(COLUMN_PASSWORD_TYPE);
  DCHECK(type_int >= 0 && type_int <= PasswordForm::TYPE_GENERATED);
  form->type = static_cast<PasswordForm::Type>(type_int);
  Pickle pickle(
      static_cast<const char*>(s.ColumnBlob(COLUMN_POSSIBLE_USERNAMES)),
      s.ColumnByteLength(COLUMN_POSSIBLE_USERNAMES));
  form->other_possible_usernames = DeserializeVector(pickle);
  form->times_used = s.ColumnInt(COLUMN_TIMES_USED);
  /*Pickle form_data_pickle(
      static_cast<const char*>(s.ColumnBlob(COLUMN_FORM_DATA)),
      s.ColumnByteLength(COLUMN_FORM_DATA));
  PickleIterator form_data_iter(form_data_pickle);
  autofill::DeserializeFormData(&form_data_iter, &form->form_data);*/
  return ENCRYPTION_RESULT_SUCCESS;
}

bool LoginDatabase::GetAllLoginsWithBlacklistSetting(
    bool blacklisted, std::vector<PasswordForm*>* forms) const {
  DCHECK(forms);
  // You *must* change LoginTableColumns if this query changes.
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
    "SELECT origin_url, action_url, "
    "username_element, username_value, "
    "password_element, password_value, submit_element, "
    "signon_realm, ssl_valid, preferred, date_created, blacklisted_by_user, "
    "scheme, password_type, possible_usernames, times_used "
    "FROM logins WHERE blacklisted_by_user == ? "
    "ORDER BY origin_url"));
  s.BindInt(0, blacklisted ? 1 : 0);

  while (s.Step()) {
    std::unique_ptr<PasswordForm> new_form(new PasswordForm());
    EncryptionResult result = InitPasswordFormFromStatement(new_form.get(), s);
    if (result == ENCRYPTION_RESULT_SERVICE_FAILURE)
      return false;
    if (result == ENCRYPTION_RESULT_ITEM_FAILURE)
      continue;
    DCHECK(result == ENCRYPTION_RESULT_SUCCESS);
    forms->push_back(new_form.release());
  }
  return s.Succeeded();
}

LoginDatabase::EncryptionResult LoginDatabase::DecryptedString(
    const std::string& cipher_text,
    string16* plain_text) const {
	if (OSCrypt::DecryptString16(cipher_text, plain_text))
    return ENCRYPTION_RESULT_SUCCESS;
  return ENCRYPTION_RESULT_ITEM_FAILURE;
}

std::vector<string16> LoginDatabase::DeserializeVector(const Pickle& p) const {
  std::vector<string16> ret;
  string16 str;

  PickleIterator iterator(p);
  while (iterator.ReadString16(&str)) {
    ret.push_back(str);
  }
  return ret;
}

} // namespace importer 