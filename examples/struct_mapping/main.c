/*
 * JSON <-> C struct mapping: no manual tree walking.
 * Tip: `nyxj gen-struct --name Order sample.json` writes these tables for you.
 */
#include "../../include/nyxj.h"
#include "../../include/nyxj_map.h"
#include "../../include/nyxj_schema.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char city[32];
    char country[3];
} Shipping;

typedef struct {
    int id;
    char* customer;
    double total;
    bool paid;
    Shipping shipping;
    nyxj_value* items; /* Free-form array kept as a JSON value */
} Order;

static const nyxj_field shipping_fields[] = {
    NYXJ_FIELD_STRBUF(Shipping, city, "city", true),
    NYXJ_FIELD_STRBUF(Shipping, country, "country", true),
};

static const nyxj_field order_fields[] = {
    NYXJ_FIELD_INT(Order, id, "id", true),
    NYXJ_FIELD_STRING(Order, customer, "customer", true),
    NYXJ_FIELD_DOUBLE(Order, total, "total", true),
    NYXJ_FIELD_BOOL(Order, paid, "paid", false),
    NYXJ_FIELD_OBJECT(Order, shipping, "shipping", true, shipping_fields),
    NYXJ_FIELD_VALUE(Order, items, "items", false),
};

int main(void) {
    const char* json =
        "{\"id\": 1042, \"customer\": \"Ada Lovelace\", \"total\": 99.9, \"paid\": true,"
        " \"shipping\": {\"city\": \"Kinshasa\", \"country\": \"CD\"},"
        " \"items\": [{\"sku\": \"NX-1\", \"qty\": 2}, {\"sku\": \"NX-9\", \"qty\": 1}]}";

    /* Validate first: reject bad input before it touches the struct. */
    const char* schema = "{\"type\":\"object\",\"required\":[\"id\",\"total\"],\"properties\":{\"total\":{\"minimum\":0}}}";
    nyxj_schema_report report;
    nyxj_error err;
    if (!nyxj_schema_validate_json(schema, json, &report, &err)) {
        fprintf(stderr, "invalid order: %s\n", report.count ? report.issues[0].message : err.message);
        return 1;
    }

    Order order = {0};
    if (!nyxj_map_parse(json, order_fields, NYXJ_FIELDS_COUNT(order_fields), &order, &err)) {
        fprintf(stderr, "mapping failed: %s\n", err.message);
        return 1;
    }
    printf("Order #%d for %s: %.2f (%s), ship to %s/%s, %d line(s)\n", order.id, order.customer, order.total,
           order.paid ? "paid" : "unpaid", order.shipping.city, order.shipping.country, nyxj_size(order.items));

    /* And back to JSON */
    order.paid = false;
    char* out = nyxj_struct_to_json(&order, order_fields, NYXJ_FIELDS_COUNT(order_fields), 2);
    printf("%s\n", out);
    free(out);
    nyxj_map_free(&order, order_fields, NYXJ_FIELDS_COUNT(order_fields));
    return 0;
}
