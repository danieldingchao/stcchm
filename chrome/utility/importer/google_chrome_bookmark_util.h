#ifndef CHROME_UTILITY_IMPORTER_GOOGLE_CHROME_BOOKMARK_UTIL_H_
#define CHROME_UTILITY_IMPORTER_GOOGLE_CHROME_BOOKMARK_UTIL_H_
#pragma once

#include <set>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/md5.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/favicon_base/favicon_types.h"
#include "components/autofill/core/common/password_form.h"
#include "sql/connection.h"
#include "sql/meta_table.h"
#include "ui/base/models/tree_node_model.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace bookmarks {
  class BookmarkModel;
}

using namespace base;
#define OVERRIDE override

namespace importer {
//
// NOTE!!!: Copy from chrome/browser/bookmarks/bookmark_model.h. Please sync it 
// if the other changes.

// BookmarkNode ---------------------------------------------------------------

// BookmarkNode contains information about a starred entry: title, URL, favicon,
// id and type. BookmarkNodes are returned from BookmarkModel.
  class BookmarkNode : public ui::TreeNode<BookmarkNode> {
  public:
    enum Type {
      URL,
      FOLDER,
      BOOKMARK_BAR,
      OTHER_NODE,
      MOBILE
    };

    enum FaviconState {
      INVALID_FAVICON,
      LOADING_FAVICON,
      LOADED_FAVICON,
    };

    typedef std::map<std::string, std::string> MetaInfoMap;

    static const int64_t kInvalidSyncTransactionVersion;

    // Creates a new node with an id of 0 and |url|.
    explicit BookmarkNode(const GURL& url);
    // Creates a new node with |id| and |url|.
    BookmarkNode(int64_t id, const GURL& url);

    ~BookmarkNode() override;

    // Set the node's internal title. Note that this neither invokes observers
    // nor updates any bookmark model this node may be in. For that functionality,
    // BookmarkModel::SetTitle(..) should be used instead.
    void SetTitle(const base::string16& title) override;

    // Returns an unique id for this node.
    // For bookmark nodes that are managed by the bookmark model, the IDs are
    // persisted across sessions.
    int64_t id() const { return id_; }
    void set_id(int64_t id) { id_ = id; }

    const GURL& url() const { return url_; }
    void set_url(const GURL& url) { url_ = url; }

    // Returns the favicon's URL. Returns an empty URL if there is no favicon
    // associated with this bookmark.
    const GURL& icon_url() const { return icon_url_; }

    Type type() const { return type_; }
    void set_type(Type type) { type_ = type; }

    // Returns the time the node was added.
    const base::Time& date_added() const { return date_added_; }
    void set_date_added(const base::Time& date) { date_added_ = date; }

    // Returns the last time the folder was modified. This is only maintained
    // for folders (including the bookmark bar and other folder).
    const base::Time& date_folder_modified() const {
      return date_folder_modified_;
    }
    void set_date_folder_modified(const base::Time& date) {
      date_folder_modified_ = date;
    }

    // Convenience for testing if this node represents a folder. A folder is a
    // node whose type is not URL.
    bool is_folder() const { return type_ != URL; }
    bool is_url() const { return type_ == URL; }

    bool is_favicon_loaded() const { return favicon_state_ == LOADED_FAVICON; }

    // Accessor method for controlling the visibility of a bookmark node/sub-tree.
    // Note that visibility is not propagated down the tree hierarchy so if a
    // parent node is marked as invisible, a child node may return "Visible". This
    // function is primarily useful when traversing the model to generate a UI
    // representation but we may want to suppress some nodes.
    virtual bool IsVisible() const;

    // Gets/sets/deletes value of |key| in the meta info represented by
    // |meta_info_str_|. Return true if key is found in meta info for gets or
    // meta info is changed indeed for sets/deletes.
    bool GetMetaInfo(const std::string& key, std::string* value) const;
    bool SetMetaInfo(const std::string& key, const std::string& value);
    bool DeleteMetaInfo(const std::string& key);
    void SetMetaInfoMap(const MetaInfoMap& meta_info_map);
    // Returns NULL if there are no values in the map.
    const MetaInfoMap* GetMetaInfoMap() const;

    void set_sync_transaction_version(int64_t sync_transaction_version) {
      sync_transaction_version_ = sync_transaction_version;
    }
    int64_t sync_transaction_version() const { return sync_transaction_version_; }

    // TODO(sky): Consider adding last visit time here, it'll greatly simplify
    // HistoryContentsProvider.

  private:
    friend class bookmarks::BookmarkModel;

    // A helper function to initialize various fields during construction.
    void Initialize(int64_t id);

    // Called when the favicon becomes invalid.
    void InvalidateFavicon();

    // Sets the favicon's URL.
    void set_icon_url(const GURL& icon_url) {
      icon_url_ = icon_url;
    }

    // Returns the favicon. In nearly all cases you should use the method
    // BookmarkModel::GetFavicon rather than this one as it takes care of
    // loading the favicon if it isn't already loaded.
    const gfx::Image& favicon() const { return favicon_; }
    void set_favicon(const gfx::Image& icon) { favicon_ = icon; }

    favicon_base::IconType favicon_type() const { return favicon_type_; }
    void set_favicon_type(favicon_base::IconType type) { favicon_type_ = type; }

    FaviconState favicon_state() const { return favicon_state_; }
    void set_favicon_state(FaviconState state) { favicon_state_ = state; }

    base::CancelableTaskTracker::TaskId favicon_load_task_id() const {
      return favicon_load_task_id_;
    }
    void set_favicon_load_task_id(base::CancelableTaskTracker::TaskId id) {
      favicon_load_task_id_ = id;
    }

    // The unique identifier for this node.
    int64_t id_;

    // The URL of this node. BookmarkModel maintains maps off this URL, so changes
    // to the URL must be done through the BookmarkModel.
    GURL url_;

    // The type of this node. See enum above.
    Type type_;

    // Date of when this node was created.
    base::Time date_added_;

    // Date of the last modification. Only used for folders.
    base::Time date_folder_modified_;

    // The favicon of this node.
    gfx::Image favicon_;

    // The type of favicon currently loaded.
    favicon_base::IconType favicon_type_;

    // The URL of the node's favicon.
    GURL icon_url_;

    // The loading state of the favicon.
    FaviconState favicon_state_;

    // If not base::CancelableTaskTracker::kBadTaskId, it indicates
    // we're loading the
    // favicon and the task is tracked by CancelabelTaskTracker.
    base::CancelableTaskTracker::TaskId favicon_load_task_id_;

    // A map that stores arbitrary meta information about the node.
    std::unique_ptr<MetaInfoMap> meta_info_map_;

    // The sync transaction version. Defaults to kInvalidSyncTransactionVersion.
    int64_t sync_transaction_version_;

    DISALLOW_COPY_AND_ASSIGN(BookmarkNode);
  };

// -----------------------------------------------------------------------------
// NOTE!!!: Copy from chrome/browser/bookmarks/bookmark_codec.h. Please sync it 
// if the other changes.

// BookmarkCodec is responsible for encoding and decoding the BookmarkModel
// into JSON values. The encoded values are written to disk via the
// BookmarkStorage.
  class BookmarkCodec {
  public:
    // Creates an instance of the codec. During decoding, if the IDs in the file
    // are not unique, we will reassign IDs to make them unique. There are no
    // guarantees on how the IDs are reassigned or about doing minimal
    // reassignments to achieve uniqueness.
    BookmarkCodec();
    ~BookmarkCodec();


    // Decodes the previously encoded value to the specified nodes as well as
    // setting |max_node_id| to the greatest node id. Returns true on success,
    // false otherwise. If there is an error (such as unexpected version) all
    // children are removed from the bookmark bar and other folder nodes. On exit
    // |max_node_id| is set to the max id of the nodes.
    bool Decode(BookmarkNode* bb_node,
      BookmarkNode* other_folder_node,
      BookmarkNode* mobile_folder_node,
      int64_t* max_node_id,
      const base::Value& value);

    // Returns the checksum computed during last encoding/decoding call.
    const std::string& computed_checksum() const { return computed_checksum_; }

    // Returns the checksum that's stored in the file. After a call to Encode,
    // the computed and stored checksums are the same since the computed checksum
    // is stored to the file. After a call to decode, the computed checksum can
    // differ from the stored checksum if the file contents were changed by the
    // user.
    const std::string& stored_checksum() const { return stored_checksum_; }

    // Return meta info of bookmark model root.
    const BookmarkNode::MetaInfoMap& model_meta_info_map() const {
      return model_meta_info_map_;
    }

    // Return the sync transaction version of the bookmark model root.
    int64_t model_sync_transaction_version() const {
      return model_sync_transaction_version_;
    }

    // Returns whether the IDs were reassigned during decoding. Always returns
    // false after encoding.
    bool ids_reassigned() const { return ids_reassigned_; }

    // Names of the various keys written to the Value.
    static const char kRootsKey[];
    static const char kRootFolderNameKey[];
    static const char kOtherBookmarkFolderNameKey[];
    static const char kMobileBookmarkFolderNameKey[];
    static const char kVersionKey[];
    static const char kChecksumKey[];
    static const char kIdKey[];
    static const char kTypeKey[];
    static const char kNameKey[];
    static const char kDateAddedKey[];
    static const char kURLKey[];
    static const char kDateModifiedKey[];
    static const char kChildrenKey[];
    static const char kMetaInfo[];
    static const char kSyncTransactionVersion[];

    // Possible values for kTypeKey.
    static const char kTypeURL[];
    static const char kTypeFolder[];

  private:
    // Encodes node and all its children into a Value object and returns it.
    std::unique_ptr<base::Value> EncodeNode(const BookmarkNode* node);

    // Encodes the given meta info into a Value object and returns it. The caller
    // takes ownership of the returned object.
    base::Value* EncodeMetaInfo(const BookmarkNode::MetaInfoMap& meta_info_map);

    // Helper to perform decoding.
    bool DecodeHelper(BookmarkNode* bb_node,
      BookmarkNode* other_folder_node,
      BookmarkNode* mobile_folder_node,
      const base::Value& value);

    // Decodes the children of the specified node. Returns true on success.
    bool DecodeChildren(const base::ListValue& child_value_list,
      BookmarkNode* parent);

    // Reassigns bookmark IDs for all nodes.
    void ReassignIDs(BookmarkNode* bb_node,
      BookmarkNode* other_node,
      BookmarkNode* mobile_node);

    // Helper to recursively reassign IDs.
    void ReassignIDsHelper(BookmarkNode* node);

    // Decodes the supplied node from the supplied value. Child nodes are
    // created appropriately by way of DecodeChildren. If node is NULL a new
    // node is created and added to parent (parent must then be non-NULL),
    // otherwise node is used.
    bool DecodeNode(const base::DictionaryValue& value,
      BookmarkNode* parent,
      BookmarkNode* node);

    // Decodes the meta info from the supplied value. If the meta info contains
    // a "sync.transaction_version" key, the value of that field will be stored
    // in the sync_transaction_version variable, then deleted. This is for
    // backward-compatibility reasons.
    // meta_info_map and sync_transaction_version must not be NULL.
    bool DecodeMetaInfo(const base::DictionaryValue& value,
      BookmarkNode::MetaInfoMap* meta_info_map,
      int64_t* sync_transaction_version);

    // Decodes the meta info from the supplied sub-node dictionary. The values
    // found will be inserted in meta_info_map with the given prefix added to the
    // start of their keys.
    void DecodeMetaInfoHelper(const base::DictionaryValue& dict,
      const std::string& prefix,
      BookmarkNode::MetaInfoMap* meta_info_map);

    // Updates the check-sum with the given string.
    void UpdateChecksum(const std::string& str);
    void UpdateChecksum(const base::string16& str);

    // Updates the check-sum with the given contents of URL/folder bookmark node.
    // NOTE: These functions take in individual properties of a bookmark node
    // instead of taking in a BookmarkNode for efficiency so that we don't convert
    // various data-types to UTF16 strings multiple times - once for serializing
    // and once for computing the check-sum.
    // The url parameter should be a valid UTF8 string.
    void UpdateChecksumWithUrlNode(const std::string& id,
      const base::string16& title,
      const std::string& url);
    void UpdateChecksumWithFolderNode(const std::string& id,
      const base::string16& title);

    // Initializes/Finalizes the checksum.
    void InitializeChecksum();
    void FinalizeChecksum();

    // Whether or not IDs were reassigned by the codec.
    bool ids_reassigned_;

    // Whether or not IDs are valid. This is initially true, but set to false
    // if an id is missing or not unique.
    bool ids_valid_;

    // Contains the id of each of the nodes found in the file. Used to determine
    // if we have duplicates.
    std::set<int64_t> ids_;

    // MD5 context used to compute MD5 hash of all bookmark data.
    base::MD5Context md5_context_;

    // Checksums.
    std::string computed_checksum_;
    std::string stored_checksum_;

    // Maximum ID assigned when decoding data.
    int64_t maximum_id_;

    // Meta info set on bookmark model root.
    BookmarkNode::MetaInfoMap model_meta_info_map_;

    // Sync transaction version set on bookmark model root.
    int64_t model_sync_transaction_version_;

    DISALLOW_COPY_AND_ASSIGN(BookmarkCodec);
  };

// -----------------------------------------------------------------------------
// NOTE!!!: Copy from chrome/browser/password_manager/login_database.h. Please 
// sync it if the other changes.

// Interface to the database storage of login information, intended as a helper
// for PasswordStore on platforms that need internal storage of some or all of
// the login information.
class LoginDatabase {
 public:
  LoginDatabase();
  virtual ~LoginDatabase();

  // Initialize the database with an sqlite file at the given path.
  // If false is returned, no other method should be called.
  bool Init(const base::FilePath& db_path);

  // Loads the complete list of autofillable password forms (i.e., not blacklist
  // entries) into |forms|.
  bool GetAutofillableLogins(
      std::vector<autofill::PasswordForm*>* forms) const;

  // Loads the complete list of blacklist forms into |forms|.
  bool GetBlacklistLogins(
      std::vector<autofill::PasswordForm*>* forms) const;

 private:
  // Result values for encryption/decryption actions.
  enum EncryptionResult {
    // Success.
    ENCRYPTION_RESULT_SUCCESS,
    // Failure for a specific item (e.g., the encrypted value was manually
    // moved from another machine, and can't be decrypted on this machine).
    // This is presumed to be a permanent failure.
    ENCRYPTION_RESULT_ITEM_FAILURE,
    // A service-level failure (e.g., on a platform using a keyring, the keyring
    // is temporarily unavailable).
    // This is presumed to be a temporary failure.
    ENCRYPTION_RESULT_SERVICE_FAILURE,
  };

  // Decrypts cipher_text, setting the value of plain_text and returning true if
  // successful, or returning false and leaving plain_text unchanged if
  // decryption fails (e.g., if the underlying OS encryption system is
  // temporarily unavailable).
  EncryptionResult DecryptedString(const std::string& cipher_text,
                                   string16* plain_text) const;

  bool InitLoginsTable();

  // Fills |form| from the values in the given statement (which is assumed to
  // be of the form used by the Get*Logins methods).
  // Returns the EncryptionResult from decrypting the password in |s|; if not
  // ENCRYPTION_RESULT_SUCCESS, |form| is not filled.
  EncryptionResult InitPasswordFormFromStatement(autofill::PasswordForm* form,
                                                 sql::Statement& s) const;

  // Loads all logins whose blacklist setting matches |blacklisted| into
  // |forms|.
  bool GetAllLoginsWithBlacklistSetting(
      bool blacklisted, std::vector<autofill::PasswordForm*>* forms) const;

  std::vector<string16> DeserializeVector(const Pickle& pickle) const;

  base::FilePath db_path_;
  mutable sql::Connection db_;
  sql::MetaTable meta_table_;

  DISALLOW_COPY_AND_ASSIGN(LoginDatabase);
};

} // namespace importer

#endif // CHROME_UTILITY_IMPORTER_GOOGLE_CHROME_BOOKMARK_UTIL_H_