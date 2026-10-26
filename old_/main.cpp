#include "TPV.h"
#include <iostream>
#include <filesystem>
#include <limits>
#include <string>

using namespace tpv;

namespace {
Cents readMoney(const std::string& prompt) {
    while (true) {
        std::cout << prompt << " (euros, ej. 12.50): ";
        std::string value; std::cin >> value;
        for (char& c : value) if (c == ',') c = '.';
        try {
            const auto dot = value.find('.');
            const std::string eurosText = dot == std::string::npos ? value : value.substr(0,dot);
            const std::string centsText = dot == std::string::npos ? "" : value.substr(dot+1);
            if (eurosText.empty() || centsText.size() > 2) throw std::invalid_argument("importe");
            long long euros = std::stoll(eurosText);
            long long cents = centsText.empty() ? 0 : std::stoll(centsText);
            if (centsText.size() == 1) cents *= 10;
            if (euros < 0 || cents < 0) throw std::invalid_argument("negativo");
            return euros*100+cents;
        } catch (...) { std::cout << "Importe no válido.\n"; }
    }
}
}

int main() {
    try {
        std::filesystem::create_directories("data");
        TPV tpv("data/tpv.db");
        std::cout << "TPV backend de pruebas (no es un sistema fiscal certificado)\n";
        bool running = true;
        while (running) {
            std::cout << "\n1) Listar artículos  2) Crear artículo  3) Cobro manual  4) Venta por artículo\n"
                         "5) Abrir caja  6) Resumen caja  7) Entrada caja  8) Salida caja\n"
                         "9) Cerrar caja  10) Configurar impresora  11) Imprimir último ticket  0) Salir\n> ";
            int choice{}; if (!(std::cin >> choice)) break;
            try {
                if (choice == 0) running = false;
                else if (choice == 1) {
                    for (const auto& a : tpv.listArticles()) std::cout << a.id << ") " << a.name << " - " << formatMoney(a.priceCents) << '\n';
                } else if (choice == 2) {
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); std::string name;
                    std::cout << "Nombre: "; std::getline(std::cin,name); const auto price=readMoney("Precio");
                    std::cout << "Artículo creado con ID " << tpv.createArticle(name,price) << '\n';
                } else if (choice == 3) {
                    const auto amount=readMoney("Importe a cobrar"); std::cout << "Concepto: "; std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n'); std::string desc; std::getline(std::cin,desc); if(desc.empty()) desc="Cobro manual";
                    std::cout << "Método (1 efectivo, 2 tarjeta, 3 otro): "; int m; std::cin>>m; const auto method=m==2?PaymentMethod::Card:(m==3?PaymentMethod::Other:PaymentMethod::Cash);
                    const auto tendered=method==PaymentMethod::Cash?readMoney("Efectivo entregado"):amount;
                    const auto r=tpv.quickCharge(amount,method,tendered,desc); std::cout << "Venta "<<r.saleId<<" cobrada. Total "<<formatMoney(r.totalCents)<<", cambio "<<formatMoney(r.changeCents)<<'\n';
                } else if (choice == 4) {
                    const auto articles=tpv.listArticles(); for(const auto& a:articles) std::cout<<a.id<<") "<<a.name<<" - "<<formatMoney(a.priceCents)<<'\n';
                    std::cout<<"ID artículo: "; Id articleId; std::cin>>articleId; auto sale=tpv.createSale(); tpv.addArticle(sale,articleId);
                    const auto total=tpv.getSale(sale).totalCents; std::cout<<"Total: "<<formatMoney(total)<<"\nMétodo (1 efectivo, 2 tarjeta, 3 otro): "; int m; std::cin>>m; const auto method=m==2?PaymentMethod::Card:(m==3?PaymentMethod::Other:PaymentMethod::Cash);
                    const auto tendered=method==PaymentMethod::Cash?readMoney("Efectivo entregado"):total; const auto r=tpv.pay(sale,method,tendered); std::cout<<"Venta "<<r.saleId<<" cobrada. Cambio "<<formatMoney(r.changeCents)<<'\n';
                } else if (choice == 5) {
                    const auto opening=readMoney("Fondo inicial"); std::cout<<"Caja abierta, sesión "<<tpv.openCashRegister(opening)<<'\n';
                } else if (choice == 6) {
                    const auto s=tpv.cashSummary(); std::cout<<"Fondo: "<<formatMoney(s.openingCents)<<"\nVentas efectivo: "<<formatMoney(s.cashSalesCents)<<"\nEntradas: "<<formatMoney(s.cashInCents)<<"\nSalidas: "<<formatMoney(s.cashOutCents)<<"\nEsperado: "<<formatMoney(s.expectedCents)<<'\n';
                } else if (choice == 7 || choice == 8) {
                    const auto amount=readMoney("Importe"); std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n'); std::string reason; std::cout<<"Motivo: "; std::getline(std::cin,reason); if(choice==7)tpv.cashIn(amount,reason);else tpv.cashOut(amount,reason); std::cout<<"Movimiento registrado.\n";
                } else if (choice == 9) {
                    const auto counted=readMoney("Efectivo contado"); tpv.closeCashRegister(counted); std::cout<<"Caja cerrada.\n";
                } else if (choice == 10) {
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n'); std::string name; std::cout<<"Nombre de impresora en Windows: "; std::getline(std::cin,name); tpv.printer().setPrinterName(name); std::cout<<"Impresora configurada para esta ejecución.\n";
                } else if (choice == 11) {
                    std::cout<<"ID de venta: "; Id id; std::cin>>id; std::string error; if(!tpv.printTicket(id,"Mi bar",error))std::cout<<"Error de impresión: "<<error<<'\n'; else std::cout<<"Trabajo enviado a la impresora.\n";
                } else std::cout<<"Opción desconocida.\n";
            } catch(const std::exception& e) { std::cout<<"Error: "<<e.what()<<'\n'; }
        }
    } catch(const std::exception& e) { std::cerr<<"Error fatal: "<<e.what()<<'\n'; return 1; }
    return 0;
}
