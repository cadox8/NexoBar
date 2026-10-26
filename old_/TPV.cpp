#include "TPV.h"
#include "../TicketTemplate.h"
#include <limits>
#include <stdexcept>

namespace tpv {
namespace {
void require(bool condition, const std::string& message) { if (!condition) throw std::invalid_argument(message); }
std::string methodToDb(PaymentMethod method) { return paymentMethodName(method); }
}

TPV::TPV(const std::string& path) : db_(path) { db_.initializeSchema(); }

Id TPV::createArticle(const std::string& name, Cents price) {
    require(!name.empty(), "El nombre del artículo no puede estar vacío.");
    require(price >= 0, "El precio no puede ser negativo.");
    Statement q(db_.handle(), "INSERT INTO articles(name,price_cents) VALUES(?,?)");
    q.bindText(1, name); q.bindInt(2, price); q.stepDone();
    return sqlite3_last_insert_rowid(db_.handle());
}
void TPV::updateArticle(Id id, const std::string& name, Cents price) {
    require(!name.empty() && price >= 0, "Nombre o precio no válido.");
    Statement q(db_.handle(), "UPDATE articles SET name=?,price_cents=? WHERE id=?");
    q.bindText(1, name); q.bindInt(2, price); q.bindInt(3, id); q.stepDone();
    if (sqlite3_changes(db_.handle()) == 0) throw std::runtime_error("No existe el artículo.");
}
void TPV::deactivateArticle(Id id) {
    Statement q(db_.handle(), "UPDATE articles SET active=0 WHERE id=?"); q.bindInt(1, id); q.stepDone();
    if (sqlite3_changes(db_.handle()) == 0) throw std::runtime_error("No existe el artículo.");
}
std::vector<Article> TPV::listArticles(bool activeOnly) const {
    Statement q(db_.handle(), activeOnly ? "SELECT id,name,price_cents,active FROM articles WHERE active=1 ORDER BY name" : "SELECT id,name,price_cents,active FROM articles ORDER BY name");
    std::vector<Article> result;
    while (q.stepRow()) result.push_back({q.int64(0), q.text(1), q.int64(2), q.int64(3) != 0});
    return result;
}
Id TPV::createSale() {
    Statement q(db_.handle(), "INSERT INTO sales(status,total_cents) VALUES('OPEN',0)"); q.stepDone();
    return sqlite3_last_insert_rowid(db_.handle());
}
void TPV::requireOpenSale(Id id) const {
    Statement q(db_.handle(), "SELECT status FROM sales WHERE id=?"); q.bindInt(1, id);
    if (!q.stepRow()) throw std::runtime_error("No existe la venta.");
    if (q.text(0) != "OPEN") throw std::runtime_error("La venta ya no está abierta.");
}
void TPV::addArticle(Id saleId, Id articleId, std::int64_t quantityMilli) {
    require(quantityMilli > 0, "La cantidad debe ser mayor que cero."); requireOpenSale(saleId);
    Statement a(db_.handle(), "SELECT name,price_cents FROM articles WHERE id=? AND active=1"); a.bindInt(1, articleId);
    if (!a.stepRow()) throw std::runtime_error("Artículo inexistente o inactivo.");
    const auto name = a.text(0); const auto price = a.int64(1);
    Statement q(db_.handle(), "INSERT INTO sale_items(sale_id,article_id,description,quantity_milli,unit_price_cents,total_cents) VALUES(?,?,?,?,?,?)");
    q.bindInt(1, saleId); q.bindInt(2, articleId); q.bindText(3, name); q.bindInt(4, quantityMilli); q.bindInt(5, price); q.bindInt(6, lineTotal(price, quantityMilli)); q.stepDone();
    recalculateSale(saleId);
}
void TPV::addCustomItem(Id saleId, const std::string& description, Cents price, std::int64_t quantityMilli) {
    require(!description.empty(), "El concepto no puede estar vacío."); require(price >= 0, "El importe no puede ser negativo."); require(quantityMilli > 0, "La cantidad debe ser mayor que cero."); requireOpenSale(saleId);
    Statement q(db_.handle(), "INSERT INTO sale_items(sale_id,article_id,description,quantity_milli,unit_price_cents,total_cents) VALUES(?,NULL,?,?,?,?)");
    q.bindInt(1, saleId); q.bindText(2, description); q.bindInt(3, quantityMilli); q.bindInt(4, price); q.bindInt(5, lineTotal(price, quantityMilli)); q.stepDone();
    recalculateSale(saleId);
}
void TPV::recalculateSale(Id id) {
    Statement q(db_.handle(), "UPDATE sales SET total_cents=(SELECT COALESCE(SUM(total_cents),0) FROM sale_items WHERE sale_id=?) WHERE id=? AND status='OPEN'");
    q.bindInt(1, id); q.bindInt(2, id); q.stepDone();
}
Sale TPV::getSale(Id id) const {
    Statement s(db_.handle(), "SELECT id,created_at,status,total_cents FROM sales WHERE id=?"); s.bindInt(1, id);
    if (!s.stepRow()) throw std::runtime_error("No existe la venta.");
    Sale sale{s.int64(0), s.text(1), s.text(2), s.int64(3), {}};
    Statement q(db_.handle(), "SELECT id,article_id,description,quantity_milli,unit_price_cents,total_cents FROM sale_items WHERE sale_id=? ORDER BY id"); q.bindInt(1, id);
    while (q.stepRow()) {
        SaleLine line; line.id=q.int64(0); if (!q.isNull(1)) line.articleId=q.int64(1); line.description=q.text(2); line.quantityMilli=q.int64(3); line.unitPriceCents=q.int64(4); line.totalCents=q.int64(5); sale.lines.push_back(std::move(line));
    }
    return sale;
}
PaymentResult TPV::pay(Id id, PaymentMethod method, Cents tendered) {
    require(tendered >= 0, "El importe entregado no puede ser negativo.");
    Transaction tx(db_); requireOpenSale(id);
    const auto sale = getSale(id);
    if (sale.lines.empty() || sale.totalCents <= 0) throw std::runtime_error("No se puede cobrar una venta vacía o de importe cero.");
    if (method != PaymentMethod::Cash && tendered < sale.totalCents) tendered = sale.totalCents;
    if (method == PaymentMethod::Cash && tendered < sale.totalCents) throw std::runtime_error("El efectivo entregado es inferior al total.");
    const Cents change = method == PaymentMethod::Cash ? tendered - sale.totalCents : 0;
    Statement p(db_.handle(), "INSERT INTO payments(sale_id,method,amount_cents,tendered_cents,change_cents) VALUES(?,?,?,?,?)");
    p.bindInt(1,id); p.bindText(2,methodToDb(method)); p.bindInt(3,sale.totalCents); p.bindInt(4,tendered); p.bindInt(5,change); p.stepDone();
    Statement u(db_.handle(), "UPDATE sales SET status='PAID',paid_cents=?,change_cents=? WHERE id=? AND status='OPEN'");
    u.bindInt(1,sale.totalCents); u.bindInt(2,change); u.bindInt(3,id); u.stepDone();
    tx.commit();
    return {true,{},id,sale.totalCents,tendered,change};
}
PaymentResult TPV::quickCharge(Cents amount, PaymentMethod method, Cents tendered, const std::string& description) {
    require(amount > 0, "El importe debe ser mayor que cero.");
    const Id id = createSale();
    try { addCustomItem(id, description, amount); return pay(id, method, tendered); }
    catch (...) { throw; }
}
Id TPV::activeCashSession() const {
    Statement q(db_.handle(), "SELECT id FROM cash_sessions WHERE closed_at IS NULL ORDER BY id DESC LIMIT 1");
    if (!q.stepRow()) throw std::runtime_error("No hay una caja abierta.");
    return q.int64(0);
}
Id TPV::openCashRegister(Cents opening) {
    require(opening >= 0, "El fondo inicial no puede ser negativo.");
    Statement active(db_.handle(), "SELECT id FROM cash_sessions WHERE closed_at IS NULL LIMIT 1");
    if (active.stepRow()) throw std::runtime_error("Ya hay una caja abierta.");
    Statement q(db_.handle(), "INSERT INTO cash_sessions(opening_cents) VALUES(?)"); q.bindInt(1,opening); q.stepDone();
    return sqlite3_last_insert_rowid(db_.handle());
}
void TPV::cashIn(Cents amount, const std::string& reason) {
    require(amount > 0 && !reason.empty(), "Importe y motivo son obligatorios."); const auto id=activeCashSession();
    Statement q(db_.handle(), "INSERT INTO cash_movements(session_id,direction,amount_cents,reason) VALUES(?,'IN',?,?)"); q.bindInt(1,id); q.bindInt(2,amount); q.bindText(3,reason); q.stepDone();
}
void TPV::cashOut(Cents amount, const std::string& reason) {
    require(amount > 0 && !reason.empty(), "Importe y motivo son obligatorios."); const auto id=activeCashSession();
    Statement q(db_.handle(), "INSERT INTO cash_movements(session_id,direction,amount_cents,reason) VALUES(?,'OUT',?,?)"); q.bindInt(1,id); q.bindInt(2,amount); q.bindText(3,reason); q.stepDone();
}
CashSummary TPV::cashSummary() const {
    const auto id=activeCashSession();
    Statement s(db_.handle(), "SELECT opening_cents FROM cash_sessions WHERE id=?"); s.bindInt(1,id); if(!s.stepRow()) throw std::runtime_error("No existe la caja.");
    const Cents opening=s.int64(0);
    Statement sales(db_.handle(), "SELECT COALESCE(SUM(p.amount_cents),0) FROM payments p JOIN sales s ON s.id=p.sale_id WHERE s.status='PAID' AND p.method='EFECTIVO' AND p.created_at >= (SELECT opened_at FROM cash_sessions WHERE id=?)"); sales.bindInt(1,id); sales.stepRow(); const Cents cashSales=sales.int64(0);
    Statement moves(db_.handle(), "SELECT COALESCE(SUM(CASE WHEN direction='IN' THEN amount_cents ELSE 0 END),0),COALESCE(SUM(CASE WHEN direction='OUT' THEN amount_cents ELSE 0 END),0) FROM cash_movements WHERE session_id=?"); moves.bindInt(1,id); moves.stepRow();
    const Cents in=moves.int64(0), out=moves.int64(1);
    return {id,opening,cashSales,in,out,opening+cashSales+in-out};
}
void TPV::closeCashRegister(Cents counted) {
    require(counted >= 0, "El efectivo contado no puede ser negativo."); const auto summary=cashSummary();
    Statement q(db_.handle(), "UPDATE cash_sessions SET closed_at=strftime('%Y-%m-%dT%H:%M:%fZ','now'),counted_cents=?,expected_cents=? WHERE id=? AND closed_at IS NULL");
    q.bindInt(1,counted); q.bindInt(2,summary.expectedCents); q.bindInt(3,summary.sessionId); q.stepDone();
}
bool TPV::printTicket(Id id, const std::string& businessName, std::string& error, const std::string& footer) {
    const auto sale=getSale(id);
    std::string content;
    try { content = TicketTemplate::renderFile("resources/tickets/ticket.tpl", sale, businessName, footer); }
    catch (const std::runtime_error&) { content = TicketTemplate::renderDefault(sale, businessName, footer); }
    content += "\n\n\n\x1D" "V" "\x00";
    return printer_.printRaw(std::string("\x1B" "@") + content, error);
}
} // namespace tpv
