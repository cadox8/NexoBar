#pragma once

#include <string>
#include <vector>

namespace tpv {

struct IntegrityReport {
    bool ok{false};
    std::vector<std::string> messages;
};

class StorageMaintenance {
public:
    // Comprueba PRAGMA integrity_check y PRAGMA foreign_key_check.
    // No modifica la base de datos.
    static IntegrityReport verify(const std::string& databasePath);

    // Crea una copia consistente usando la API de backup online de SQLite.
    // Puede ejecutarse con la base abierta por la aplicación.
    static void backup(const std::string& sourcePath, const std::string& backupPath);

    // Restaura la copia en una ruta DESTINO distinta. Por seguridad no sobrescribe
    // el archivo de origen; el llamador debe cerrar la aplicación y decidir cómo
    // sustituir la base de datos activa después de verificar el destino.
    static void restore(const std::string& backupPath, const std::string& destinationPath);
};

} // namespace tpv
