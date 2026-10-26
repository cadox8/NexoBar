#include "database/Orm.h"
#include "NamedType/named_type.hpp"

#include <gtest/gtest.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace test {
    using CategoryId = fluent::NamedType<std::int64_t, struct CategoryIdTag, fluent::Comparable>;
    using ProductId = fluent::NamedType<std::int64_t, struct ProductIdTag, fluent::Comparable>;

    struct Money {
        std::int64_t cents{};
    };

    enum class Kind { Food, Drink };

    struct Category {
        CategoryId id{};
        std::string name;
    };

    using CategoryTable = Nearon::Table<"categories", Category,
        Nearon::Column<"id", &Category::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"name", &Category::name, Nearon::Unique>>;
    NEARON_TABLE(CategoryTable);

    struct Product {
        ProductId id{};
        CategoryId categoryId{};
        std::string name;
        std::optional<std::string> barcode;
        bool active{true};
        Money price;
        Kind kind{Kind::Food};
    };

    using ProductTable = Nearon::Table<"products", Product,
        Nearon::Column<"id", &Product::id, Nearon::PrimaryKey>,
        Nearon::Column<"category_id", &Product::categoryId, Nearon::ForeignKey<&Category::id, Nearon::FkAction::Cascade>>,
        Nearon::Column<"name", &Product::name>,
        Nearon::Column<"barcode", &Product::barcode>,
        Nearon::Column<"active", &Product::active>,
        Nearon::Column<"price_cents", &Product::price>,
        Nearon::Column<"kind", &Product::kind>,
        Nearon::UniqueKey<&Product::categoryId, &Product::name>,
        Nearon::Index<"idx_products_barcode", &Product::barcode>>;
    NEARON_TABLE(ProductTable);

    /// Class with private members, mapped from inside.
    class Note {
    public:
        Note() = default;
        Note(std::string text) : m_text(std::move(text)) {}

        std::int64_t id() const { return m_id; }
        const std::string& text() const { return m_text; }

        NEARON_TABLE_FRIEND("notes", Note,
            Nearon::Column<"id", &Note::m_id, Nearon::PrimaryKey>,
            Nearon::Column<"text", &Note::m_text>);

    private:
        std::int64_t m_id{};
        std::string m_text;
    };
}

template<>
struct Nearon::ValueCodec<test::Money> {
    using storage_type = std::int64_t;
    static constexpr bool enabled = true;

    static storage_type encode(const test::Money& value) { return value.cents; }
    static test::Money decode(const storage_type value) { return {value}; }
};

TEST(OrmTests, RunsScenarios) {
    using namespace test;
    using Nearon::col;

    auto db = std::make_unique<Nearon::Database>(":memory:");
    db->createTables<Category, Product, Note>();
    db->createTables<Category, Product, Note>(); // idempotent

    Category drinks{CategoryId{0}, "Bebidas"};
    const CategoryId drinksId = db->insert<Category>(drinks);
    EXPECT_TRUE(drinks.id == drinksId && drinksId.get() > 0);
    const CategoryId foodId = db->insert<Category>(Category{CategoryId{0}, "Comida"});

    Product coffee{ProductId{0}, drinksId, "Café", std::nullopt, true, Money{150}, Kind::Drink};
    const ProductId coffeeId = db->insert<Product>(coffee);
    EXPECT_TRUE(coffee.id == coffeeId);

    const auto stored = db->select<Product>(coffeeId);
    EXPECT_TRUE(stored && stored->name == "Café" && !stored->barcode && stored->price.cents == 150 && stored->kind == Kind::Drink);

    auto updated = *stored;
    updated.barcode = "840000000001";
    EXPECT_TRUE(db->update<Product>(updated));
    EXPECT_TRUE(db->get<Product>(coffeeId).barcode == "840000000001");

    const auto ids = db->insertMany<Product>(std::vector<Product>{
        {ProductId{0}, drinksId, "Caña", std::nullopt, true, Money{200}, Kind::Drink},
        {ProductId{0}, foodId, "Tostada", std::nullopt, false, Money{180}, Kind::Food},
    });
    EXPECT_TRUE(ids.size() == 2);
    EXPECT_TRUE(db->count<Product>() == 3);

    // Duplicate (category, name) violates the UNIQUE key.
    bool threw = false;
    try {
        db->insert<Product>(Product{ProductId{0}, drinksId, "Café", std::nullopt, true, Money{1}, Kind::Drink});
    } catch (const Nearon::ConstraintError&) {
        threw = true;
    }
    EXPECT_TRUE(threw);

    // Queries
    const auto activeDrinks = db->query<Product>()
            .where(col<&Product::active> == true && col<&Product::kind> == Kind::Drink)
            .orderBy(col<&Product::price>.desc())
            .all();
    EXPECT_TRUE(activeDrinks.size() == 2 && activeDrinks[0].name == "Caña");

    EXPECT_TRUE(db->selectWhere<Product>(col<&Product::barcode>.isNull()).size() == 2);
    EXPECT_TRUE(db->selectWhere<Product>(col<&Product::name>.like("Ca%")).size() == 2);
    EXPECT_TRUE(db->selectWhere<Product>(col<&Product::id>.in({coffeeId, ids[1]})).size() == 2);
    EXPECT_TRUE(db->selectWhere<Product>(Nearon::field<Product, "price_cents"> >= Money{180}).size() == 2);
    EXPECT_TRUE(db->sum<&Product::price>(col<&Product::active> == true) == 350);
    EXPECT_TRUE(db->query<Product>().limit(1).offset(1).all().size() == 1);

    // Joins
    const auto rows = db->join<Product, Category>(col<&Product::categoryId> == col<&Category::id>)
            .where(col<&Category::name> == "Bebidas")
            .orderBy(col<&Product::name>.asc())
            .all();
    EXPECT_TRUE(rows.size() == 2 && std::get<1>(rows[0]).name == "Bebidas");

    const CategoryId emptyId = db->insert<Category>(Category{CategoryId{0}, "Vacía"});
    const auto withProducts = db->query<Category>()
            .leftJoin<Product>(col<&Product::categoryId> == col<&Category::id>)
            .where(col<&Category::id> == emptyId)
            .all();
    EXPECT_TRUE(withProducts.size() == 1 && !std::get<1>(withProducts[0]));

    // Union
    const auto united = db->query<Product>().where(col<&Product::kind> == Kind::Food)
            .unionAll(db->query<Product>().where(col<&Product::price> < Money{160}))
            .all();
    EXPECT_TRUE(united.size() == 2);

    // Update / delete builders
    EXPECT_TRUE(db->update<Product>().set(col<&Product::active>, false).where(col<&Product::categoryId> == drinksId).exec() == 2);
    EXPECT_TRUE(db->update<Product>().set(col<&Product::barcode>, std::nullopt).exec() == 3);
    EXPECT_TRUE(db->count<Product>(col<&Product::active> == true) == 0);

    // Transactions
    try {
        db->transaction([&] {
            db->insert<Category>(Category{CategoryId{0}, "Temporal"});
            throw std::runtime_error("rollback");
        });
    } catch (const std::runtime_error&) {}
    EXPECT_TRUE(!db->selectFirst<Category>(col<&Category::name> == "Temporal"));

    {
        auto tx = db->transaction();
        db->insert<Category>(Category{CategoryId{0}, "Postres"});
        {
            auto nested = db->transaction();
            db->insert<Category>(Category{CategoryId{0}, "Anidada"});
        } // nested rolled back
        tx.commit();
    }
    EXPECT_TRUE(db->selectFirst<Category>(col<&Category::name> == "Postres"));
    EXPECT_TRUE(!db->selectFirst<Category>(col<&Category::name> == "Anidada"));

    // Foreign key cascade
    EXPECT_TRUE(db->remove<Category>(drinksId));
    EXPECT_TRUE(db->count<Product>() == 1);

    // Upsert
    Category food = db->get<Category>(foodId);
    food.name = "Cocina";
    db->upsert<Category>(food);
    EXPECT_TRUE(db->get<Category>(foodId).name == "Cocina");

    // Private members
    Note note("privado");
    const auto noteId = db->insert<Note>(note);
    EXPECT_TRUE(note.id() == noteId);
    EXPECT_TRUE(db->select<Note>(noteId)->text() == "privado");
    EXPECT_TRUE(db->remove<Note>(note));
    EXPECT_TRUE(!db->exists<Note>(noteId));

    // Raw queries
    const auto names = db->queryRaw<std::string, std::int64_t>("SELECT name, price_cents FROM products");
    EXPECT_TRUE(names.size() == 1 && std::get<0>(names[0]) == "Tostada");
    EXPECT_TRUE(db->scalar<std::int64_t>("SELECT COUNT(*) FROM categories WHERE name <> ?", std::string("x")) == 3);

}
