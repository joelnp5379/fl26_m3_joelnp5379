#include "aiws/gui/main_window.hpp"
#include "aiws/gui/workspace_widget.hpp"
#include "aiws/gui/search_widget.hpp"
#include "aiws/gui/term_inspector_widget.hpp"
#include "aiws/gui/context_widget.hpp"

#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace aiws::gui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    auto* central_widget = new QWidget(this);
    setCentralWidget(central_widget);
    auto* main_layout = new QVBoxLayout(central_widget);

    auto* header_label = new QLabel("<b>AI Workspace</b><br><i>Document Search and Context Explorer</i>", this);
    header_label->setStyleSheet("background-color: #7B1FA2; color: white; padding: 10px; font-size: 16px;");
    
    workspace_widget_ = new WorkspaceWidget(this);
    search_widget_ = new SearchWidget(this);
    term_inspector_widget_ = new TermInspectorWidget(this);
    context_widget_ = new ContextWidget(this);

    auto* bottom_layout = new QHBoxLayout();
    status_label_ = new QLabel("Current status: Ready", this);
    reset_button_ = new QPushButton("Reset", this);
    exit_button_ = new QPushButton("Exit", this);

    reset_button_->setStyleSheet("background-color: #F57C00; color: white; font-weight: bold; padding: 5px;");
    exit_button_->setStyleSheet("background-color: #1976D2; color: white; font-weight: bold; padding: 5px;");

    bottom_layout->addWidget(status_label_);
    bottom_layout->addStretch();
    bottom_layout->addWidget(reset_button_);
    bottom_layout->addWidget(exit_button_);

    auto* tools_layout = new QHBoxLayout();
    tools_layout->addWidget(term_inspector_widget_, 1);
    tools_layout->addWidget(context_widget_, 1);

    main_layout->addWidget(header_label);
    main_layout->addWidget(workspace_widget_, 1);
    main_layout->addWidget(search_widget_, 2);
    main_layout->addLayout(tools_layout, 1);
    main_layout->addLayout(bottom_layout);

    connect(workspace_widget_, &WorkspaceWidget::add_document_requested, this, &MainWindow::add_document);
    connect(workspace_widget_, &WorkspaceWidget::build_corpus_requested, this, &MainWindow::build_corpus);
    connect(workspace_widget_, &WorkspaceWidget::document_selected, this, &MainWindow::document_selected);
    
    connect(search_widget_, &SearchWidget::search_requested, this, &MainWindow::search_requested);
    connect(search_widget_, &SearchWidget::query_changed, this, &MainWindow::query_changed);
    connect(search_widget_, &SearchWidget::source_document_requested, this, &MainWindow::source_document_requested);
    
    connect(search_widget_, &SearchWidget::selected_chunk_changed, term_inspector_widget_, &TermInspectorWidget::set_selected_chunk);

    connect(context_widget_, &ContextWidget::build_context_requested, this, &MainWindow::build_context_requested);
    connect(term_inspector_widget_, &TermInspectorWidget::term_inspection_requested, this, &MainWindow::term_inspection_requested);
    
    connect(reset_button_, &QPushButton::clicked, this, &MainWindow::reset_application);
    connect(exit_button_, &QPushButton::clicked, this, &MainWindow::exit_application);

    resize(900, 900);
    reset_application();
}

void MainWindow::set_exit_callback(ExitCallback callback) noexcept {
    exit_callback_ = callback;
}

void MainWindow::add_document() {
    QString file_path = QFileDialog::getOpenFileName(this, "Select Document", "", "Text Files (*.txt);;All Files (*)");
    if (file_path.isEmpty()) return;

    QFile file(file_path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString content = in.readAll();
        
        QFileInfo file_info(file_path);
        QString title = file_info.fileName();
        
        workspace_.add_document(Document{file_path.toStdString(), title.toStdString(), content.toStdString()});
        
        workspace_widget_->add_document(title);
        invalidate_corpus();
        update_action_states();
    }
}

void MainWindow::build_corpus() {
    try {
        processing_core_.rebuild(workspace_);
        corpus_current_ = true;
        workspace_widget_->set_corpus_status(QString("Corpus: %1 chunks").arg(processing_core_.chunk_count()));
        clear_processing_outputs();
        status_label_->setText("Current status: Corpus built successfully.");
    } catch (const std::exception& e) {
        status_label_->setText(QString("Current status: Rebuild failed - %1").arg(e.what()));
    }
    update_action_states();
}

void MainWindow::document_selected(int row) {
    if (row >= 0 && row < static_cast<int>(workspace_.documents().size())) {
        const auto& doc = workspace_.documents()[row];
        workspace_widget_->show_selected_document(QString::fromStdString(doc.title()), QString::fromStdString(doc.text()));
    }
}

void MainWindow::search_requested(const QString& query, int k) {
    if (!corpus_current_) return;
    
    auto results = processing_core_.search(query.toStdString(), k);
    search_widget_->set_results(std::move(results));
    
    context_widget_->clear_context();
    status_label_->setText("Current status: Search completed.");
}

void MainWindow::query_changed(const QString& query) {
    (void)query;
    update_action_states();
}

void MainWindow::build_context_requested(int token_budget) {
    if (!corpus_current_) return;
    
    auto items = processing_core_.build_context(search_widget_->query().toStdString(), search_widget_->result_count(), token_budget);
    context_widget_->set_context(items);
    status_label_->setText("Current status: Context built.");
}

void MainWindow::source_document_requested(const QString& document_id) {
    std::string id_str = document_id.toStdString();
    const auto& docs = workspace_.documents();
    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (docs[i].id() == id_str) {
            workspace_widget_->select_document(static_cast<int>(i));
            break;
        }
    }
}

void MainWindow::term_inspection_requested(const QString& term, const QString& chunk_id) {
    if (!corpus_current_ || term.trimmed().isEmpty()) return;
    
    std::string term_str = term.toStdString();
    try {
        std::size_t df = processing_core_.document_frequency(term_str);
        bool has_chunk = !chunk_id.isEmpty();
        std::size_t tf = 0;
        
        if (has_chunk) {
            tf = processing_core_.term_frequency(term_str, chunk_id.toStdString());
        }
        
        term_inspector_widget_->set_statistics(df, has_chunk, tf);
    } catch (...) {
        term_inspector_widget_->clear_statistics();
    }
}

void MainWindow::reset_application() {
    workspace_.clear();
    corpus_current_ = false;
    
    Resettable* components[] = {workspace_widget_, search_widget_, term_inspector_widget_, context_widget_};
    for (auto* comp : components) {
        comp->reset();
    }
    
    status_label_->setText("Current status: Application reset.");
    update_action_states();
}

void MainWindow::exit_application() {
    if (exit_callback_) {
        exit_callback_(this);
    }
}

void MainWindow::update_action_states() {
    workspace_widget_->set_build_corpus_enabled(!workspace_.documents().empty());
    
    bool can_search = corpus_current_ && search_widget_->has_query();
    search_widget_->set_search_enabled(can_search);
    context_widget_->set_build_context_enabled(can_search);
    
    term_inspector_widget_->set_inspection_enabled(corpus_current_);
}

void MainWindow::invalidate_corpus() {
    corpus_current_ = false;
    workspace_widget_->set_corpus_status("Corpus: Not built");
    clear_processing_outputs();
}

void MainWindow::clear_processing_outputs() {
    search_widget_->clear_results();
    term_inspector_widget_->clear_statistics();
    context_widget_->clear_context();
}

}  // namespace aiws::gui