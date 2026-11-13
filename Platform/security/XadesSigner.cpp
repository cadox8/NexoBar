#include "XadesSigner.h"

#include "Sha256.h"

#include <libxml/c14n.h>
#include <libxml/parser.h>
#include <libxml/xmlschemas.h>
#include <libxml/tree.h>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include <wincrypt.h>
#else
#include <openssl/store.h>
#endif

namespace nexobar {
    namespace {
        constexpr auto DS = "http://www.w3.org/2000/09/xmldsig#";
        constexpr auto XADES = "http://uri.etsi.org/01903/v1.3.2#";
        constexpr auto C14N = "http://www.w3.org/TR/2001/REC-xml-c14n-20010315";
        constexpr auto RSA_SHA256 = "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256";
        constexpr auto SHA256_URI = "http://www.w3.org/2001/04/xmlenc#sha256";
        constexpr auto SHA1_URI = "http://www.w3.org/2000/09/xmldsig#sha1";
        constexpr auto ENVELOPED = "http://www.w3.org/2000/09/xmldsig#enveloped-signature";
        constexpr auto SIGNED_PROPERTIES = "http://uri.etsi.org/01903#SignedProperties";
        constexpr auto POLICY_OID = "urn:oid:2.16.724.1.3.1.1.2.1.9";
        constexpr auto POLICY_URL = "https://sede.administracion.gob.es/politica_de_firma_anexo_1.pdf";
        constexpr auto POLICY_DIGEST = "G7roucf600+f03r/o0bAOQ6WAs0=";

        using Doc = std::unique_ptr<xmlDoc, decltype(&xmlFreeDoc)>;
        using Bio = std::unique_ptr<BIO, decltype(&BIO_free)>;
        using P12 = std::unique_ptr<PKCS12, decltype(&PKCS12_free)>;
        using Cert = std::unique_ptr<X509, decltype(&X509_free)>;
        using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
        using Md = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

        [[noreturn]] void fail(const std::string& message) {
            const unsigned long code = ERR_peek_last_error();
            if (!code) throw std::runtime_error(message);
            char detail[256]{};
            ERR_error_string_n(code, detail, sizeof(detail));
            throw std::runtime_error(message + ": " + detail);
        }

        Doc parse(const std::string_view xml) {
            auto* raw = xmlReadMemory(xml.data(), static_cast<int>(xml.size()), nullptr, "UTF-8",
                                      XML_PARSE_NONET | XML_PARSE_NOBLANKS);
            if (!raw) throw std::runtime_error("errors.fiscal_xml_parse");
            return Doc(raw, xmlFreeDoc);
        }

        std::string base64(const unsigned char* bytes, const int length) {
            std::string result(static_cast<std::size_t>(4 * ((length + 2) / 3)), '\0');
            const int written = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(result.data()), bytes, length);
            result.resize(static_cast<std::size_t>(written));
            return result;
        }

        std::vector<unsigned char> unbase64(std::string value) {
            value.erase(std::remove_if(value.begin(), value.end(), [](const unsigned char c) {
                return c == ' ' || c == '\r' || c == '\n' || c == '\t';
            }), value.end());
            std::vector<unsigned char> result(value.size() * 3 / 4 + 3);
            const int written = EVP_DecodeBlock(result.data(),
                reinterpret_cast<const unsigned char*>(value.data()), static_cast<int>(value.size()));
            if (written < 0) throw std::runtime_error("errors.fiscal_signature_base64");
            std::size_t size = static_cast<std::size_t>(written);
            while (!value.empty() && value.back() == '=') { --size; value.pop_back(); }
            result.resize(size);
            return result;
        }

        std::string digest(const std::string_view value, const EVP_MD* algorithm) {
            unsigned char output[EVP_MAX_MD_SIZE]{};
            unsigned int length = 0;
            if (EVP_Digest(value.data(), value.size(), output, &length, algorithm, nullptr) != 1)
                fail("errors.fiscal_digest");
            return base64(output, static_cast<int>(length));
        }

        std::string canonicalDocument(xmlDoc* doc) {
            xmlChar* output = nullptr;
            const int length = xmlC14NDocDumpMemory(doc, nullptr, XML_C14N_1_0, nullptr, 0, &output);
            if (length < 0 || !output) throw std::runtime_error("errors.fiscal_canonicalization");
            std::string result(reinterpret_cast<const char*>(output), static_cast<std::size_t>(length));
            xmlFree(output);
            return result;
        }

        std::string canonicalNode(xmlNode* node) {
            Doc standalone(xmlNewDoc(BAD_CAST "1.0"), xmlFreeDoc);
            xmlNode* copy = xmlDocCopyNode(node, standalone.get(), 1);
            xmlDocSetRootElement(standalone.get(), copy);
            xmlReconciliateNs(standalone.get(), copy);
            return canonicalDocument(standalone.get());
        }

        std::string serialize(xmlDoc* doc) {
            xmlChar* output = nullptr;
            int length = 0;
            xmlDocDumpFormatMemoryEnc(doc, &output, &length, "UTF-8", 0);
            if (!output) throw std::runtime_error("errors.fiscal_xml_serialize");
            std::string result(reinterpret_cast<const char*>(output), static_cast<std::size_t>(length));
            xmlFree(output);
            return result;
        }

        std::string serialize(xmlNode* node) {
            xmlBufferPtr buffer = xmlBufferCreate();
            if (!buffer) throw std::runtime_error("errors.fiscal_xml_serialize");
            xmlNodeDump(buffer, node->doc, node, 0, 0);
            std::string result(reinterpret_cast<const char*>(buffer->content), buffer->use);
            xmlBufferFree(buffer);
            return result;
        }

        xmlNode* child(xmlNode* parent, xmlNs* ns, const char* name, const std::string_view value = {}) {
            return xmlNewChild(parent, ns, BAD_CAST name,
                value.empty() ? nullptr : BAD_CAST std::string(value).c_str());
        }

        xmlNode* find(xmlNode* node, const char* name, const char* href = nullptr) {
            for (xmlNode* current = node; current; current = current->next) {
                if (current->type == XML_ELEMENT_NODE && xmlStrEqual(current->name, BAD_CAST name) &&
                    (!href || current->ns && xmlStrEqual(current->ns->href, BAD_CAST href))) return current;
                if (auto* nested = find(current->children, name, href)) return nested;
            }
            return nullptr;
        }

        std::string content(xmlNode* node) {
            if (!node) return {};
            xmlChar* value = xmlNodeGetContent(node);
            std::string result = value ? reinterpret_cast<const char*>(value) : "";
            if (value) xmlFree(value);
            return result;
        }

        std::string nameText(X509_NAME* name) {
            Bio bio(BIO_new(BIO_s_mem()), BIO_free);
            if (!bio || X509_NAME_print_ex(bio.get(), name, 0, XN_FLAG_RFC2253) < 0)
                fail("errors.fiscal_certificate_name");
            const char* data = nullptr;
            const long length = BIO_get_mem_data(bio.get(), &data);
            return std::string(data, static_cast<std::size_t>(length));
        }

        std::string serialText(X509* certificate) {
            std::unique_ptr<BIGNUM, decltype(&BN_free)> number(
                ASN1_INTEGER_to_BN(X509_get_serialNumber(certificate), nullptr), BN_free);
            if (!number) fail("errors.fiscal_certificate_serial");
            char* decimal = BN_bn2dec(number.get());
            if (!decimal) fail("errors.fiscal_certificate_serial");
            std::string result(decimal);
            OPENSSL_free(decimal);
            return result;
        }

        std::string certificateDer(X509* certificate) {
            const int length = i2d_X509(certificate, nullptr);
            if (length <= 0) fail("errors.fiscal_certificate_der");
            std::string result(static_cast<std::size_t>(length), '\0');
            unsigned char* cursor = reinterpret_cast<unsigned char*>(result.data());
            if (i2d_X509(certificate, &cursor) != length) fail("errors.fiscal_certificate_der");
            return result;
        }

        std::string thumbprint(X509* certificate) {
            unsigned char bytes[EVP_MAX_MD_SIZE]{};
            unsigned int length = 0;
            if (X509_digest(certificate, EVP_sha256(), bytes, &length) != 1)
                fail("errors.fiscal_certificate_digest");
            static constexpr char HEX[] = "0123456789ABCDEF";
            std::string result;
            for (unsigned i = 0; i < length; ++i) {
                result += HEX[bytes[i] >> 4];
                result += HEX[bytes[i] & 15];
            }
            return result;
        }

        bool isQualifiedCertificate(X509* certificate) {
            std::unique_ptr<ASN1_OBJECT, decltype(&ASN1_OBJECT_free)> qcStatements(
                OBJ_txt2obj("1.3.6.1.5.5.7.1.3", 1), ASN1_OBJECT_free);
            if (!qcStatements) return false;
            const int index = X509_get_ext_by_OBJ(certificate, qcStatements.get(), -1);
            if (index < 0) return false;
            X509_EXTENSION* extension = X509_get_ext(certificate, index);
            const ASN1_OCTET_STRING* data = extension ? X509_EXTENSION_get_data(extension) : nullptr;
            if (!data) return false;
            // ETSI id-etsi-qcs-QcCompliance: 0.4.0.1862.1.1
            static constexpr unsigned char complianceOid[] = {0x06, 0x06, 0x04, 0x00, 0x8e, 0x46, 0x01, 0x01};
            return std::search(data->data, data->data + data->length,
                               std::begin(complianceOid), std::end(complianceOid)) != data->data + data->length;
        }

        using SignatureProvider = std::function<std::vector<unsigned char>(std::string_view)>;

        XadesResult buildSignature(const std::string_view xml, X509* certificate,
                                   const std::string_view signingTime,
                                   const SignatureProvider& signBytes) {
            if (!certificate) throw std::runtime_error("errors.fiscal_certificate_key");
            if (X509_cmp_current_time(X509_get0_notBefore(certificate)) > 0 ||
                X509_cmp_current_time(X509_get0_notAfter(certificate)) < 0)
                throw std::runtime_error("errors.fiscal_certificate_expired");
            Key publicKey(X509_get_pubkey(certificate), EVP_PKEY_free);
            if (!publicKey || EVP_PKEY_base_id(publicKey.get()) != EVP_PKEY_RSA || EVP_PKEY_bits(publicKey.get()) < 2048)
                throw std::runtime_error("errors.fiscal_certificate_algorithm");

            auto document = parse(xml);
            xmlNode* root = xmlDocGetRootElement(document.get());
            if (!root) throw std::runtime_error("errors.fiscal_xml_parse");
            const std::string documentDigest = digest(canonicalDocument(document.get()), EVP_sha256());
            const std::string suffix = sha256(std::string(xml) + std::string(signingTime)).substr(0, 32);
            const std::string signatureId = "xmldsig-" + suffix;
            const std::string propertiesId = signatureId + "-signedprops";

            xmlNode* signatureParent = xmlStrEqual(root->name, BAD_CAST "RegistroEvento")
                ? find(root->children, "Evento") : root;
            if (!signatureParent) throw std::runtime_error("errors.fiscal_xml_parse");
            xmlNode* signature = child(signatureParent, nullptr, "Signature");
            xmlNs* ds = xmlNewNs(signature, BAD_CAST DS, BAD_CAST "ds");
            xmlSetNs(signature, ds);
            xmlNewProp(signature, BAD_CAST "Id", BAD_CAST signatureId.c_str());
            xmlNode* signedInfo = child(signature, ds, "SignedInfo");
            auto* canonicalization = child(signedInfo, ds, "CanonicalizationMethod");
            xmlNewProp(canonicalization, BAD_CAST "Algorithm", BAD_CAST C14N);
            auto* signatureMethod = child(signedInfo, ds, "SignatureMethod");
            xmlNewProp(signatureMethod, BAD_CAST "Algorithm", BAD_CAST RSA_SHA256);
            auto* documentReference = child(signedInfo, ds, "Reference");
            xmlNewProp(documentReference, BAD_CAST "Id", BAD_CAST (signatureId + "-ref0").c_str());
            xmlNewProp(documentReference, BAD_CAST "URI", BAD_CAST "");
            auto* transforms = child(documentReference, ds, "Transforms");
            auto* transform = child(transforms, ds, "Transform");
            xmlNewProp(transform, BAD_CAST "Algorithm", BAD_CAST ENVELOPED);
            auto* digestMethod = child(documentReference, ds, "DigestMethod");
            xmlNewProp(digestMethod, BAD_CAST "Algorithm", BAD_CAST SHA256_URI);
            child(documentReference, ds, "DigestValue", documentDigest);

            auto* propertyReference = child(signedInfo, ds, "Reference");
            xmlNewProp(propertyReference, BAD_CAST "Type", BAD_CAST SIGNED_PROPERTIES);
            xmlNewProp(propertyReference, BAD_CAST "URI", BAD_CAST ("#" + propertiesId).c_str());
            auto* propertyTransforms = child(propertyReference, ds, "Transforms");
            auto* propertyTransform = child(propertyTransforms, ds, "Transform");
            xmlNewProp(propertyTransform, BAD_CAST "Algorithm", BAD_CAST C14N);
            auto* propertyDigestMethod = child(propertyReference, ds, "DigestMethod");
            xmlNewProp(propertyDigestMethod, BAD_CAST "Algorithm", BAD_CAST SHA256_URI);
            xmlNode* propertyDigestValue = child(propertyReference, ds, "DigestValue");

            xmlNode* signatureValue = child(signature, ds, "SignatureValue");
            xmlNode* keyInfo = child(signature, ds, "KeyInfo");
            xmlNode* x509Data = child(keyInfo, ds, "X509Data");
            const std::string der = certificateDer(certificate);
            child(x509Data, ds, "X509Certificate",
                  base64(reinterpret_cast<const unsigned char*>(der.data()), static_cast<int>(der.size())));

            xmlNode* object = child(signature, ds, "Object");
            xmlNs* xades = xmlNewNs(object, BAD_CAST XADES, BAD_CAST "xades");
            xmlNode* qualifying = child(object, xades, "QualifyingProperties");
            xmlNewProp(qualifying, BAD_CAST "Target", BAD_CAST ("#" + signatureId).c_str());
            xmlNode* properties = child(qualifying, xades, "SignedProperties");
            xmlNewProp(properties, BAD_CAST "Id", BAD_CAST propertiesId.c_str());
            xmlNode* signedSignatureProperties = child(properties, xades, "SignedSignatureProperties");
            child(signedSignatureProperties, xades, "SigningTime", signingTime);
            xmlNode* signingCertificate = child(signedSignatureProperties, xades, "SigningCertificate");
            xmlNode* cert = child(signingCertificate, xades, "Cert");
            xmlNode* certDigest = child(cert, xades, "CertDigest");
            auto* certDigestMethod = child(certDigest, ds, "DigestMethod");
            xmlNewProp(certDigestMethod, BAD_CAST "Algorithm", BAD_CAST SHA256_URI);
            child(certDigest, ds, "DigestValue", digest(der, EVP_sha256()));
            xmlNode* issuerSerial = child(cert, xades, "IssuerSerial");
            child(issuerSerial, ds, "X509IssuerName", nameText(X509_get_issuer_name(certificate)));
            child(issuerSerial, ds, "X509SerialNumber", serialText(certificate));
            xmlNode* policyIdentifier = child(signedSignatureProperties, xades, "SignaturePolicyIdentifier");
            xmlNode* policyId = child(policyIdentifier, xades, "SignaturePolicyId");
            xmlNode* sigPolicyId = child(policyId, xades, "SigPolicyId");
            auto* identifier = child(sigPolicyId, xades, "Identifier", POLICY_OID);
            xmlNewProp(identifier, BAD_CAST "Qualifier", BAD_CAST "OIDAsURN");
            child(sigPolicyId, xades, "Description", "Política de firma electrónica de la AGE");
            xmlNode* policyHash = child(policyId, xades, "SigPolicyHash");
            auto* policyDigestMethod = child(policyHash, ds, "DigestMethod");
            xmlNewProp(policyDigestMethod, BAD_CAST "Algorithm", BAD_CAST SHA1_URI);
            child(policyHash, ds, "DigestValue", POLICY_DIGEST);
            xmlNode* policyQualifiers = child(policyId, xades, "SigPolicyQualifiers");
            xmlNode* policyQualifier = child(policyQualifiers, xades, "SigPolicyQualifier");
            child(policyQualifier, xades, "SPURI", POLICY_URL);

            xmlNodeSetContent(propertyDigestValue, BAD_CAST digest(canonicalNode(properties), EVP_sha256()).c_str());
            const auto signatureBytes = signBytes(canonicalNode(signedInfo));
            xmlNodeSetContent(signatureValue,
                BAD_CAST base64(signatureBytes.data(), static_cast<int>(signatureBytes.size())).c_str());
            return {serialize(document.get()), serialize(signature), nameText(X509_get_subject_name(certificate)),
                    thumbprint(certificate)};
        }
    }

    XadesResult XadesSigner::sign(const std::string_view xml,
                                  const std::filesystem::path& pkcs12Path,
                                  const std::string_view password,
                                  const std::string_view signingTime) {
        ERR_clear_error();
        Bio input(BIO_new_file(pkcs12Path.string().c_str(), "rb"), BIO_free);
        if (!input) throw std::runtime_error("errors.fiscal_certificate_open");
        P12 bundle(d2i_PKCS12_bio(input.get(), nullptr), PKCS12_free);
        if (!bundle) fail("errors.fiscal_certificate_read");
        EVP_PKEY* rawKey = nullptr;
        X509* rawCertificate = nullptr;
        STACK_OF(X509)* chain = nullptr;
        const std::string secret(password);
        if (PKCS12_parse(bundle.get(), secret.c_str(), &rawKey, &rawCertificate, &chain) != 1)
            fail("errors.fiscal_certificate_password");
        Key key(rawKey, EVP_PKEY_free);
        Cert certificate(rawCertificate, X509_free);
        if (chain) sk_X509_pop_free(chain, X509_free);
        if (!key || !certificate) throw std::runtime_error("errors.fiscal_certificate_key");
        if (X509_cmp_current_time(X509_get0_notBefore(certificate.get())) > 0 ||
            X509_cmp_current_time(X509_get0_notAfter(certificate.get())) < 0)
            throw std::runtime_error("errors.fiscal_certificate_expired");
        if (EVP_PKEY_base_id(key.get()) != EVP_PKEY_RSA || EVP_PKEY_bits(key.get()) < 1024)
            throw std::runtime_error("errors.fiscal_certificate_algorithm");

        return buildSignature(xml, certificate.get(), signingTime, [&key](const std::string_view input) {
            Md context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
            if (!context || EVP_DigestSignInit(context.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1)
                fail("errors.fiscal_signature_init");
            std::size_t length = 0;
            if (EVP_DigestSign(context.get(), nullptr, &length,
                               reinterpret_cast<const unsigned char*>(input.data()), input.size()) != 1)
                fail("errors.fiscal_signature_size");
            std::vector<unsigned char> result(length);
            if (EVP_DigestSign(context.get(), result.data(), &length,
                               reinterpret_cast<const unsigned char*>(input.data()), input.size()) != 1)
                fail("errors.fiscal_signature_create");
            result.resize(length);
            return result;
        });

        auto document = parse(xml);
        xmlNode* root = xmlDocGetRootElement(document.get());
        if (!root) throw std::runtime_error("errors.fiscal_xml_parse");
        const std::string documentDigest = digest(canonicalDocument(document.get()), EVP_sha256());
        const std::string suffix = sha256(std::string(xml) + std::string(signingTime)).substr(0, 32);
        const std::string signatureId = "xmldsig-" + suffix;
        const std::string propertiesId = signatureId + "-signedprops";

        xmlNode* signatureParent = xmlStrEqual(root->name, BAD_CAST "RegistroEvento")
            ? find(root->children, "Evento") : root;
        if (!signatureParent) throw std::runtime_error("errors.fiscal_xml_parse");
        xmlNs* ds = xmlNewNs(signatureParent, BAD_CAST DS, BAD_CAST "ds");
        xmlNode* signature = child(signatureParent, ds, "Signature");
        xmlNewProp(signature, BAD_CAST "Id", BAD_CAST signatureId.c_str());
        xmlNode* signedInfo = child(signature, ds, "SignedInfo");
        xmlNewNs(signedInfo, BAD_CAST DS, BAD_CAST "ds");
        auto* canonicalization = child(signedInfo, ds, "CanonicalizationMethod");
        xmlNewProp(canonicalization, BAD_CAST "Algorithm", BAD_CAST C14N);
        auto* signatureMethod = child(signedInfo, ds, "SignatureMethod");
        xmlNewProp(signatureMethod, BAD_CAST "Algorithm", BAD_CAST RSA_SHA256);
        auto* documentReference = child(signedInfo, ds, "Reference");
        xmlNewProp(documentReference, BAD_CAST "Id", BAD_CAST (signatureId + "-ref0").c_str());
        xmlNewProp(documentReference, BAD_CAST "URI", BAD_CAST "");
        auto* transforms = child(documentReference, ds, "Transforms");
        auto* transform = child(transforms, ds, "Transform");
        xmlNewProp(transform, BAD_CAST "Algorithm", BAD_CAST ENVELOPED);
        auto* digestMethod = child(documentReference, ds, "DigestMethod");
        xmlNewProp(digestMethod, BAD_CAST "Algorithm", BAD_CAST SHA256_URI);
        child(documentReference, ds, "DigestValue", documentDigest);

        auto* propertyReference = child(signedInfo, ds, "Reference");
        xmlNewProp(propertyReference, BAD_CAST "Type", BAD_CAST SIGNED_PROPERTIES);
        xmlNewProp(propertyReference, BAD_CAST "URI", BAD_CAST ("#" + propertiesId).c_str());
        auto* propertyTransforms = child(propertyReference, ds, "Transforms");
        auto* propertyTransform = child(propertyTransforms, ds, "Transform");
        xmlNewProp(propertyTransform, BAD_CAST "Algorithm", BAD_CAST C14N);
        auto* propertyDigestMethod = child(propertyReference, ds, "DigestMethod");
        xmlNewProp(propertyDigestMethod, BAD_CAST "Algorithm", BAD_CAST SHA256_URI);
        xmlNode* propertyDigestValue = child(propertyReference, ds, "DigestValue");

        xmlNode* signatureValue = child(signature, ds, "SignatureValue");
        xmlNode* keyInfo = child(signature, ds, "KeyInfo");
        xmlNode* x509Data = child(keyInfo, ds, "X509Data");
        const std::string der = certificateDer(certificate.get());
        child(x509Data, ds, "X509Certificate",
              base64(reinterpret_cast<const unsigned char*>(der.data()), static_cast<int>(der.size())));

        xmlNode* object = child(signature, ds, "Object");
        xmlNs* xades = xmlNewNs(object, BAD_CAST XADES, BAD_CAST "xades");
        xmlNode* qualifying = child(object, xades, "QualifyingProperties");
        xmlNewProp(qualifying, BAD_CAST "Target", BAD_CAST ("#" + signatureId).c_str());
        xmlNode* properties = child(qualifying, xades, "SignedProperties");
        xmlNewNs(properties, BAD_CAST XADES, BAD_CAST "xades");
        xmlNewNs(properties, BAD_CAST DS, BAD_CAST "ds");
        xmlNewProp(properties, BAD_CAST "Id", BAD_CAST propertiesId.c_str());
        xmlNode* signedSignatureProperties = child(properties, xades, "SignedSignatureProperties");
        child(signedSignatureProperties, xades, "SigningTime", signingTime);
        xmlNode* signingCertificate = child(signedSignatureProperties, xades, "SigningCertificate");
        xmlNode* cert = child(signingCertificate, xades, "Cert");
        xmlNode* certDigest = child(cert, xades, "CertDigest");
        auto* certDigestMethod = child(certDigest, ds, "DigestMethod");
        xmlNewProp(certDigestMethod, BAD_CAST "Algorithm", BAD_CAST SHA256_URI);
        child(certDigest, ds, "DigestValue", digest(der, EVP_sha256()));
        xmlNode* issuerSerial = child(cert, xades, "IssuerSerial");
        child(issuerSerial, ds, "X509IssuerName", nameText(X509_get_issuer_name(certificate.get())));
        child(issuerSerial, ds, "X509SerialNumber", serialText(certificate.get()));
        xmlNode* policyIdentifier = child(signedSignatureProperties, xades, "SignaturePolicyIdentifier");
        xmlNode* policyId = child(policyIdentifier, xades, "SignaturePolicyId");
        xmlNode* sigPolicyId = child(policyId, xades, "SigPolicyId");
        auto* identifier = child(sigPolicyId, xades, "Identifier", POLICY_OID);
        xmlNewProp(identifier, BAD_CAST "Qualifier", BAD_CAST "OIDAsURN");
        child(sigPolicyId, xades, "Description", "Política de firma electrónica de la AGE");
        xmlNode* policyHash = child(policyId, xades, "SigPolicyHash");
        auto* policyDigestMethod = child(policyHash, ds, "DigestMethod");
        xmlNewProp(policyDigestMethod, BAD_CAST "Algorithm", BAD_CAST SHA1_URI);
        child(policyHash, ds, "DigestValue", POLICY_DIGEST);
        xmlNode* policyQualifiers = child(policyId, xades, "SigPolicyQualifiers");
        xmlNode* policyQualifier = child(policyQualifiers, xades, "SigPolicyQualifier");
        child(policyQualifier, xades, "SPURI", POLICY_URL);

        xmlNodeSetContent(propertyDigestValue, BAD_CAST digest(canonicalNode(properties), EVP_sha256()).c_str());
        const std::string canonicalSignedInfo = canonicalNode(signedInfo);
        Md context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
        if (!context || EVP_DigestSignInit(context.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1)
            fail("errors.fiscal_signature_init");
        std::size_t signatureLength = 0;
        if (EVP_DigestSign(context.get(), nullptr, &signatureLength,
                           reinterpret_cast<const unsigned char*>(canonicalSignedInfo.data()), canonicalSignedInfo.size()) != 1)
            fail("errors.fiscal_signature_size");
        std::vector<unsigned char> signatureBytes(signatureLength);
        if (EVP_DigestSign(context.get(), signatureBytes.data(), &signatureLength,
                           reinterpret_cast<const unsigned char*>(canonicalSignedInfo.data()), canonicalSignedInfo.size()) != 1)
            fail("errors.fiscal_signature_create");
        signatureBytes.resize(signatureLength);
        xmlNodeSetContent(signatureValue, BAD_CAST base64(signatureBytes.data(), static_cast<int>(signatureBytes.size())).c_str());

        return {serialize(document.get()), serialize(signature), nameText(X509_get_subject_name(certificate.get())),
                thumbprint(certificate.get())};
    }

    XadesResult XadesSigner::signSystem(const std::string_view xml,
                                        const std::string_view certificateSelector,
                                        const std::string_view signingTime) {
#ifdef _WIN32
        struct StoreCloser {
            void operator()(void* handle) const { if (handle) CertCloseStore(handle, 0); }
        };
        using Store = std::unique_ptr<void, StoreCloser>;
        using CertificateContext = std::unique_ptr<const CERT_CONTEXT, decltype(&CertFreeCertificateContext)>;
        Store currentUser(CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
            CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_READONLY_FLAG, L"MY"));
        Store localMachine(CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
            CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_READONLY_FLAG, L"MY"));
        if (!currentUser && !localMachine) throw std::runtime_error("errors.fiscal_certificate_store_open");
        auto normalized = [](const std::string_view value) {
            std::string result;
            for (const unsigned char c : value)
                if (std::isalnum(c)) result += static_cast<char>(std::toupper(c));
            return result;
        };
        const std::string wanted = normalized(certificateSelector);
        CertificateContext selected(nullptr, CertFreeCertificateContext);
        const auto inspectStore = [&](void* storeHandle) {
            PCCERT_CONTEXT current = nullptr;
            while (storeHandle && (current = CertEnumCertificatesInStore(storeHandle, current)) != nullptr) {
                const unsigned char* cursor = current->pbCertEncoded;
                Cert candidate(d2i_X509(nullptr, &cursor, current->cbCertEncoded), X509_free);
                if (!candidate || normalized(nameText(X509_get_subject_name(candidate.get()))).find(wanted) == std::string::npos)
                    continue;
                HCRYPTPROV_OR_NCRYPT_KEY_HANDLE temporary = 0;
                DWORD keySpec = 0;
                BOOL callerFree = FALSE;
                if (!CryptAcquireCertificatePrivateKey(current, CRYPT_ACQUIRE_CACHE_FLAG | CRYPT_ACQUIRE_SILENT_FLAG,
                                                       nullptr, &temporary, &keySpec, &callerFree))
                    continue;
                if (callerFree) {
                    if (keySpec == CERT_NCRYPT_KEY_SPEC) NCryptFreeObject(temporary);
                    else CryptReleaseContext(static_cast<HCRYPTPROV>(temporary), 0);
                }
                if (selected) throw std::runtime_error("errors.fiscal_certificate_multiple");
                selected.reset(CertDuplicateCertificateContext(current));
            }
        };
        inspectStore(currentUser.get());
        inspectStore(localMachine.get());
        if (!selected) throw std::runtime_error("errors.fiscal_certificate_missing_store");

        CERT_CHAIN_PARA chainParameters{};
        chainParameters.cbSize = sizeof(chainParameters);
        PCCERT_CHAIN_CONTEXT rawChain = nullptr;
        if (!CertGetCertificateChain(nullptr, selected.get(), nullptr, selected->hCertStore,
                &chainParameters, CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT, nullptr, &rawChain))
            throw std::runtime_error("errors.fiscal_certificate_chain");
        std::unique_ptr<const CERT_CHAIN_CONTEXT, decltype(&CertFreeCertificateChain)> chain(rawChain, CertFreeCertificateChain);
        CERT_CHAIN_POLICY_PARA policyParameters{};
        policyParameters.cbSize = sizeof(policyParameters);
        CERT_CHAIN_POLICY_STATUS policyStatus{};
        policyStatus.cbSize = sizeof(policyStatus);
        if (!CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_BASE, chain.get(), &policyParameters, &policyStatus) ||
            policyStatus.dwError != 0)
            throw std::runtime_error("errors.fiscal_certificate_chain");

        const unsigned char* cursor = selected->pbCertEncoded;
        Cert certificate(d2i_X509(nullptr, &cursor, selected->cbCertEncoded), X509_free);
        if (!certificate) throw std::runtime_error("errors.fiscal_certificate_read");
        if (!isQualifiedCertificate(certificate.get()))
            throw std::runtime_error("errors.fiscal_certificate_not_qualified");
        HCRYPTPROV_OR_NCRYPT_KEY_HANDLE keyHandle = 0;
        DWORD keySpec = 0;
        BOOL callerFree = FALSE;
        if (!CryptAcquireCertificatePrivateKey(selected.get(), CRYPT_ACQUIRE_CACHE_FLAG | CRYPT_ACQUIRE_SILENT_FLAG,
                                               nullptr, &keyHandle, &keySpec, &callerFree))
            throw std::runtime_error("errors.fiscal_certificate_key");
        const auto releaseKey = [&] {
            if (!callerFree) return;
            if (keySpec == CERT_NCRYPT_KEY_SPEC) NCryptFreeObject(keyHandle);
            else CryptReleaseContext(static_cast<HCRYPTPROV>(keyHandle), 0);
        };
        try {
            auto result = buildSignature(xml, certificate.get(), signingTime,
                [keyHandle, keySpec](const std::string_view input) {
                    unsigned char hash[32]{};
                    DWORD hashLength = sizeof(hash);
                    if (!CryptHashCertificate2(BCRYPT_SHA256_ALGORITHM, 0, nullptr,
                            reinterpret_cast<const unsigned char*>(input.data()), static_cast<DWORD>(input.size()),
                            hash, &hashLength))
                        throw std::runtime_error("errors.fiscal_digest");
                    if (keySpec == CERT_NCRYPT_KEY_SPEC) {
                        BCRYPT_PKCS1_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM};
                        DWORD length = 0;
                        if (NCryptSignHash(keyHandle, &padding, hash, hashLength, nullptr, 0, &length,
                                           NCRYPT_PAD_PKCS1_FLAG) != ERROR_SUCCESS)
                            throw std::runtime_error("errors.fiscal_signature_size");
                        std::vector<unsigned char> signature(length);
                        if (NCryptSignHash(keyHandle, &padding, hash, hashLength, signature.data(), length, &length,
                                           NCRYPT_PAD_PKCS1_FLAG) != ERROR_SUCCESS)
                            throw std::runtime_error("errors.fiscal_signature_create");
                        signature.resize(length);
                        return signature;
                    }
                    HCRYPTHASH cryptoHash = 0;
                    if (!CryptCreateHash(static_cast<HCRYPTPROV>(keyHandle), CALG_SHA_256, 0, 0, &cryptoHash) ||
                        !CryptSetHashParam(cryptoHash, HP_HASHVAL, hash, 0))
                        throw std::runtime_error("errors.fiscal_signature_init");
                    DWORD length = 0;
                    if (!CryptSignHashW(cryptoHash, keySpec, nullptr, 0, nullptr, &length)) {
                        CryptDestroyHash(cryptoHash);
                        throw std::runtime_error("errors.fiscal_signature_size");
                    }
                    std::vector<unsigned char> signature(length);
                    if (!CryptSignHashW(cryptoHash, keySpec, nullptr, 0, signature.data(), &length)) {
                        CryptDestroyHash(cryptoHash);
                        throw std::runtime_error("errors.fiscal_signature_create");
                    }
                    CryptDestroyHash(cryptoHash);
                    signature.resize(length);
                    std::reverse(signature.begin(), signature.end());
                    return signature;
                });
            releaseKey();
            return result;
        } catch (...) {
            releaseKey();
            throw;
        }
#else
        const char* configuredUri = std::getenv("NEXOBAR_CERTIFICATE_URI");
        const char* uri = configuredUri && *configuredUri ? configuredUri : "pkcs11:";
        using StoreContext = std::unique_ptr<OSSL_STORE_CTX, decltype(&OSSL_STORE_close)>;
        using StoreInfo = std::unique_ptr<OSSL_STORE_INFO, decltype(&OSSL_STORE_INFO_free)>;
        StoreContext store(OSSL_STORE_open(uri, nullptr, nullptr, nullptr, nullptr), OSSL_STORE_close);
        if (!store) throw std::runtime_error("errors.fiscal_certificate_store_open");
        Cert certificate(nullptr, X509_free);
        Key key(nullptr, EVP_PKEY_free);
        while (!OSSL_STORE_eof(store.get())) {
            StoreInfo info(OSSL_STORE_load(store.get()), OSSL_STORE_INFO_free);
            if (!info) continue;
            if (OSSL_STORE_INFO_get_type(info.get()) == OSSL_STORE_INFO_CERT) {
                Cert candidate(OSSL_STORE_INFO_get1_CERT(info.get()), X509_free);
                if (candidate && nameText(X509_get_subject_name(candidate.get())).find(certificateSelector) != std::string::npos)
                    certificate = std::move(candidate);
            } else if (OSSL_STORE_INFO_get_type(info.get()) == OSSL_STORE_INFO_PKEY && !key) {
                key.reset(OSSL_STORE_INFO_get1_PKEY(info.get()));
            }
        }
        if (!certificate || !key) throw std::runtime_error("errors.fiscal_certificate_missing_store");
        if (!isQualifiedCertificate(certificate.get()))
            throw std::runtime_error("errors.fiscal_certificate_not_qualified");
        return buildSignature(xml, certificate.get(), signingTime, [&key](const std::string_view input) {
            Md context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
            if (!context || EVP_DigestSignInit(context.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1)
                fail("errors.fiscal_signature_init");
            std::size_t length = 0;
            if (EVP_DigestSign(context.get(), nullptr, &length,
                               reinterpret_cast<const unsigned char*>(input.data()), input.size()) != 1)
                fail("errors.fiscal_signature_size");
            std::vector<unsigned char> signature(length);
            if (EVP_DigestSign(context.get(), signature.data(), &length,
                               reinterpret_cast<const unsigned char*>(input.data()), input.size()) != 1)
                fail("errors.fiscal_signature_create");
            signature.resize(length);
            return signature;
        });
#endif
    }

    bool XadesSigner::verify(const std::string_view signedXml, std::string& error) {
        try {
            auto document = parse(signedXml);
            xmlNode* root = xmlDocGetRootElement(document.get());
            xmlNode* signature = find(root, "Signature", DS);
            xmlNode* signedInfo = signature ? find(signature->children, "SignedInfo", DS) : nullptr;
            xmlNode* signatureValue = signature ? find(signature->children, "SignatureValue", DS) : nullptr;
            xmlNode* certificateNode = signature ? find(signature->children, "X509Certificate", DS) : nullptr;
            xmlNode* properties = signature ? find(signature->children, "SignedProperties", XADES) : nullptr;
            if (!signature || !signedInfo || !signatureValue || !certificateNode || !properties)
                throw std::runtime_error("errors.fiscal_signature_missing");
            const auto der = unbase64(content(certificateNode));
            const unsigned char* cursor = der.data();
            Cert certificate(d2i_X509(nullptr, &cursor, static_cast<long>(der.size())), X509_free);
            if (!certificate) fail("errors.fiscal_certificate_read");
            Key key(X509_get_pubkey(certificate.get()), EVP_PKEY_free);
            if (!key) fail("errors.fiscal_certificate_key");

            const auto signatureBytes = unbase64(content(signatureValue));
            const auto canonicalSignedInfo = canonicalNode(signedInfo);
            Md context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
            if (!context || EVP_DigestVerifyInit(context.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1 ||
                EVP_DigestVerify(context.get(), signatureBytes.data(), signatureBytes.size(),
                    reinterpret_cast<const unsigned char*>(canonicalSignedInfo.data()), canonicalSignedInfo.size()) != 1)
                throw std::runtime_error("errors.fiscal_signature_invalid");

            xmlNode* documentReference = nullptr;
            xmlNode* propertyReference = nullptr;
            for (xmlNode* node = signedInfo->children; node; node = node->next) {
                if (!xmlStrEqual(node->name, BAD_CAST "Reference")) continue;
                xmlChar* type = xmlGetProp(node, BAD_CAST "Type");
                if (type && xmlStrEqual(type, BAD_CAST SIGNED_PROPERTIES)) propertyReference = node;
                else documentReference = node;
                if (type) xmlFree(type);
            }
            if (!documentReference || !propertyReference) throw std::runtime_error("errors.fiscal_signature_reference");
            const auto expectedDocument = content(find(documentReference->children, "DigestValue", DS));
            const auto expectedProperties = content(find(propertyReference->children, "DigestValue", DS));
            xmlNode* signatureParent = signature->parent;
            xmlUnlinkNode(signature);
            const auto actualDocument = digest(canonicalDocument(document.get()), EVP_sha256());
            xmlAddChild(signatureParent ? signatureParent : root, signature);
            const auto actualProperties = digest(canonicalNode(properties), EVP_sha256());
            if (expectedDocument != actualDocument || expectedProperties != actualProperties)
                throw std::runtime_error("errors.fiscal_signature_digest_invalid");
            error.clear();
            return true;
        } catch (const std::exception& exception) {
            error = exception.what();
            return false;
        }
    }

    bool XadesSigner::validateSchema(const std::string_view xml,
                                     const std::filesystem::path& schemaPath,
                                     std::string& error) {
        using SchemaParser = std::unique_ptr<xmlSchemaParserCtxt, decltype(&xmlSchemaFreeParserCtxt)>;
        using Schema = std::unique_ptr<xmlSchema, decltype(&xmlSchemaFree)>;
        using SchemaValidation = std::unique_ptr<xmlSchemaValidCtxt, decltype(&xmlSchemaFreeValidCtxt)>;
        try {
            auto document = parse(xml);
            SchemaParser parser(xmlSchemaNewParserCtxt(schemaPath.string().c_str()), xmlSchemaFreeParserCtxt);
            if (!parser) throw std::runtime_error("errors.fiscal_schema_open");
            Schema schema(xmlSchemaParse(parser.get()), xmlSchemaFree);
            if (!schema) throw std::runtime_error("errors.fiscal_schema_read");
            SchemaValidation validation(xmlSchemaNewValidCtxt(schema.get()), xmlSchemaFreeValidCtxt);
            if (!validation) throw std::runtime_error("errors.fiscal_schema_read");
            if (xmlSchemaValidateDoc(validation.get(), document.get()) != 0)
                throw std::runtime_error("errors.fiscal_schema_invalid");
            error.clear();
            return true;
        } catch (const std::exception& exception) {
            error = exception.what();
            return false;
        }
    }
}
