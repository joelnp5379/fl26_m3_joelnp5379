#include "aiws/gui/search_result_model.hpp"

namespace aiws::gui {

SearchResultModel::SearchResultModel(QObject* parent)
    : QAbstractTableModel(parent) {
}

int SearchResultModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(results_.size());
}

int SearchResultModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return 5;
}

QVariant SearchResultModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= static_cast<int>(results_.size())) {
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
        const auto& res = results_[index.row()];
        switch (index.column()) {
            case 0: return index.row() + 1;
            case 1: return QString::fromStdString(res.document_id);
            case 2: return static_cast<qulonglong>(res.chunk_sequence);
            case 3: return res.score;
            case 4: return QString::fromStdString(res.text);
            default: return QVariant();
        }
    }
    return QVariant();
}

QVariant SearchResultModel::headerData(int section,
                                       Qt::Orientation orientation,
                                       int role) const {
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
            case 0: return "Rank";
            case 1: return "Document";
            case 2: return "Chunk";
            case 3: return "Score";
            case 4: return "Text";
            default: return QVariant();
        }
    }
    return QVariant();
}

void SearchResultModel::set_results(std::vector<SearchResult> results) {
    beginResetModel();
    results_ = std::move(results);
    endResetModel();
}

void SearchResultModel::clear() {
    beginResetModel();
    results_.clear();
    endResetModel();
}

const SearchResult* SearchResultModel::result_at(int row) const noexcept {
    if (row >= 0 && row < static_cast<int>(results_.size())) {
        return &results_[row];
    }
    return nullptr;
}

}  // namespace aiws::gui