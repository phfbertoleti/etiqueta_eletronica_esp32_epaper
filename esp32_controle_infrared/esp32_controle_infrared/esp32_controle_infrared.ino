/* Software: envio de etiqueta a equipamento de etiqueta eletrônica via infravermelho
 * Autor: Pedro Bertoleti
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "serial_infrared_defs.h"
#include "etiquetas_defs.h"

/* Buffer enviado para a etiqueta acordar */
const unsigned char buffer_para_acordar_etiqueta[3] = {'1','2','3'};

/* Variaveis e constantes - wi-fi */
/* Coloque aqui o nome da rede wi-fi ao qual o ESP32 precisa se conectar */
const char* ssid_wifi = " ";

/* Coloque aqui a senha da rede wi-fi ao qual o ESP32 precisa se conectar */
const char* password_wifi = " ";

WiFiClient espClient;

/* Variaveis e constantes - MQTT */
/* URL do broker MQTT */ 
const char* broker_mqtt = "mqtt.tago.io";
const char* topico_subscribe_mqtt = "tago/etiqueta_eletronica";
const char* user_mqtt = "Token";
const char* senha_mqtt = " ";  /* Coloque aqui o token do seu dispositivo na TagoIO */

/* Porta para se comunicar com broker MQTT */
int broker_port = 1883;

PubSubClient MQTT(espClient);


/* Protótipos */
unsigned short calcula_crc (unsigned char *ptr, unsigned int lng);
void init_wifi(void);
void conecta_wifi(void);
void verifica_conexao_wifi(void);
void init_MQTT(void);
void conecta_broker_MQTT(void);
void verifica_conexao_mqtt(void);
void mqtt_callback(char* topic, byte* payload, unsigned int length);

/* 
 *  Implementações
 */
/* Função: calcua CRC de um buffer desejado
 * Parâmetros: ponteiro para o buffer e tamanho do buffer
 * Retorno: CRC calculado
 */
unsigned short calcula_crc(unsigned char *ptr, unsigned int lng)
{
    unsigned int i, j;
    unsigned short CRC;

    CRC = 0xFFFF;  // initialize CRC
    for (i = 0; i < lng; i++)
    {
        CRC ^= (unsigned short)*(ptr + i);  
        for (j = 0; j < 8; j++)
        {
            if (CRC & 1)
            { 
                // test LSB
                CRC >>= 1;  
                CRC ^= 0xA001;  
            }
            else
            {
                CRC >>= 1; 
            }
        }
    }
    return CRC; 
}

/* Função: inicializa wi-fi
 * Parametros: nenhum
 * Retorno: nenhum 
 */
void init_wifi(void) 
{
    Serial.println("------WI-FI -----");
    Serial.print("Conectando-se a rede: ");
    Serial.println(ssid_wifi);
    Serial.println("Aguarde...");    
    conecta_wifi();
}

/* Função: conecta-se a rede wi-fi
 * Parametros: nenhum
 * Retorno: nenhum 
 */
void conecta_wifi(void) 
{
    /* Se ja estiver conectado, nada é feito. */
    if (WiFi.status() == WL_CONNECTED)
        return;

    /* refaz a conexão */
    WiFi.begin(ssid_wifi, password_wifi);
    
    while (WiFi.status() != WL_CONNECTED) 
    {        
        delay(50);
        Serial.print(".");
    }
  
    Serial.println();
    Serial.print("Conectado com sucesso a rede wi-fi ");
    Serial.println(ssid_wifi);
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
}

/* Função: verifica se a conexao wi-fi está ativa 
 *         (e, em caso negativo, refaz a conexao)
 * Parametros: nenhum
 * Retorno: nenhum 
 */
void verifica_conexao_wifi(void)
{
    conecta_wifi(); 
}

/* Função: inicializa MQTT
 * Parametros: nenhum
 * Retorno: nenhum 
 */
void init_MQTT(void)
{
    MQTT.setServer(broker_mqtt, broker_port);  
    MQTT.setCallback(mqtt_callback);
}

/* Função: conecta-se ao broker MQTT
 * Parametros: nenhum
 * Retorno: nenhum 
 */
void conecta_broker_MQTT(void) 
{
    char mqtt_id_randomico[5] = {0};
    int i;
    
    while (!MQTT.connected()) 
    {
        /* refaz a conexão */
        Serial.print("* Tentando se conectar ao broker MQTT: ");
        Serial.println(broker_mqtt);

        /* gera id mqtt randomico */
        randomSeed(random(9999));
        sprintf(mqtt_id_randomico, "%ld", random(9999));
        
        if (MQTT.connect(mqtt_id_randomico, user_mqtt, senha_mqtt))
        {
            Serial.println("Conexao ao broker MQTT feita com sucesso!");
            MQTT.subscribe(topico_subscribe_mqtt);
        }
        else 
            Serial.println("Falha ao se conectar ao broker MQTT");
    }
}

/* Função: verifica se a conexao ao broker MQTT está ativa 
 *         (e, em caso negativo, refaz a conexao)
 * Parametros: nenhum
 * Retorno: nenhum 
 */
void verifica_conexao_mqtt(void)
{
    conecta_broker_MQTT(); 
}

/* Função: callback de mensagem MQTT recebida
 * Parametros: dados da mensagem recebida
 * Retorno: nenhum 
 */
void mqtt_callback(char* topic, byte* payload, unsigned int length) 
{
    TEtiqueta_un_ou_kg etiqueta_un_ou_kg;
    unsigned short crc_calculado;
    unsigned char buffer_envio[255];
    int offset;
    int tamanho_buffer_envio;
    int endereco_etiqueta = 0;
    StaticJsonDocument<400> doc;
    DeserializationError erro_json = deserializeJson(doc, payload);

    Serial.println("Chegou mensagem da plataforma IoT");

    if (erro_json) 
    {
        Serial.println("[ERRO] Falha ao ler JSON");
        return;
    }

    /* Envia etiqueta */
    const char* nome_produto = doc[0]["value"];
    const char* preco_produto = doc[1]["value"];
    const char* complemento = doc[2]["value"];
    const char* id_etiqueta = doc[3]["value"];
    endereco_etiqueta = atoi(id_etiqueta);

    Serial.println("---------------");
    Serial.println("Dados da etiqueta:");
    Serial.print("ID da etiqueta: ");
    Serial.println(id_etiqueta);
    Serial.print("Nome do produto: ");
    Serial.println(nome_produto);
    Serial.print("Preço do produto: ");
    Serial.println(preco_produto);
    Serial.print("Complemento: ");
    Serial.println(complemento);
    Serial.println("---------------");
    
    memset((unsigned char *)&etiqueta_un_ou_kg, 0x00, sizeof(TEtiqueta_un_ou_kg));
    snprintf(etiqueta_un_ou_kg.nome_produto, TAMANHO_NOME_PRODUTO, "%s", nome_produto);
    snprintf(etiqueta_un_ou_kg.preco_produto, TAMANHO_PRECO_PRODUTO, "%s", preco_produto);
    snprintf(etiqueta_un_ou_kg.un_ou_kg, TAMANHO_QTDE_OU_PESO, "%s", complemento);

    frame_comunicacao.byte_1_sincronia = BYTE_1_SINCRONIA;
    frame_comunicacao.byte_2_sincronia = BYTE_2_SINCRONIA;
    frame_comunicacao.byte_3_sincronia = BYTE_3_SINCRONIA;
    frame_comunicacao.byte_4_sincronia = BYTE_4_SINCRONIA;
    frame_comunicacao.byte_5_sincronia = BYTE_5_SINCRONIA;
    frame_comunicacao.byte_6_sincronia = BYTE_6_SINCRONIA;
    frame_comunicacao.endereco_low = (unsigned char)(endereco_etiqueta & 0xFF);
    frame_comunicacao.endereco_high = (unsigned char)((endereco_etiqueta >> 8) & 0xFF);
    frame_comunicacao.tipo_etiqueta = TIPO_ETIQUETA_UNIDADE_OU_KG;
    frame_comunicacao.tamanho_dados = (unsigned char)sizeof(TEtiqueta_un_ou_kg);
    memcpy(buffer_etiqueta, (unsigned char *)&etiqueta_un_ou_kg, sizeof(TEtiqueta_un_ou_kg));
    crc_calculado = calcula_crc(buffer_etiqueta, sizeof(TEtiqueta_un_ou_kg));
    frame_comunicacao.crc_low = (unsigned char)(crc_calculado & 0x00FF);
    frame_comunicacao.crc_high = (unsigned char)((crc_calculado >> 8) & 0x00FF);

    memcpy(buffer_envio, (unsigned char *)&frame_comunicacao, sizeof(TEtiqueta_un_ou_kg) - 2);
    offset = sizeof(TEstrutura_comunicacao) - 2;
    memcpy(buffer_envio + offset, (unsigned char *)&buffer_etiqueta, sizeof(TEtiqueta_un_ou_kg));
    offset = offset + sizeof(TEtiqueta_un_ou_kg);
    memcpy(buffer_envio + offset, (unsigned char *)&crc_calculado, sizeof(unsigned short));

    /* Envia sinal para etiuqeta ter tempo de acordar */
    Serial2.write(buffer_para_acordar_etiqueta, sizeof(buffer_para_acordar_etiqueta));
    delay(2000);

    /* Envia etiqueta para o dispositivo de etiqueta eletrônica */
    tamanho_buffer_envio = sizeof(TEstrutura_comunicacao) + sizeof(TEtiqueta_un_ou_kg);
    Serial2.write(buffer_envio, tamanho_buffer_envio);
    Serial2.flush();
    Serial.println("Etiqueta enviada");
}

void setup() 
{
    /* Inicialização as seriais */
    Serial.begin(115200);
    Serial2.begin(SERIAL_IR_BAUDRATE,
                 SERIAL_IR_CONFIG,
                 SERIAL_IR_GPIO_RX,
                 SERIAL_IR_GPIO_TX);

    /* Inicializa wifi e MQTT */
    init_wifi();             
    init_MQTT();
    conecta_broker_MQTT();
}

void loop() 
{
    /* Garante a conectividade wifi e MQTT */
    verifica_conexao_wifi();
    verifica_conexao_mqtt();

    /* Faz keep-alive do MQTT */
    MQTT.loop();
     
//    /* Popula etiqueta  (tipo: unidade ou kg) e frame de comunicação */
//    Serial.println("* Enviando etiqueta do tipo unidade ou kg...");
//
//    memset((unsigned char *)&etiqueta_un_ou_kg, 0x00, sizeof(TEtiqueta_un_ou_kg));
//    sprintf(etiqueta_un_ou_kg.nome_produto, "Alcatra");
//    sprintf(etiqueta_un_ou_kg.preco_produto, "R$45,99");
//    sprintf(etiqueta_un_ou_kg.un_ou_kg, "kg");
//    
//    frame_comunicacao.byte_1_sincronia = BYTE_1_SINCRONIA;
//    frame_comunicacao.byte_2_sincronia = BYTE_2_SINCRONIA;
//    frame_comunicacao.byte_3_sincronia = BYTE_3_SINCRONIA;
//    frame_comunicacao.byte_4_sincronia = BYTE_4_SINCRONIA;
//    frame_comunicacao.byte_5_sincronia = BYTE_5_SINCRONIA;
//    frame_comunicacao.byte_6_sincronia = BYTE_6_SINCRONIA;
//    frame_comunicacao.endereco_low = ENDERECO_ETIQUETA_LOW;
//    frame_comunicacao.endereco_high = ENDERECO_ETIQUETA_HIGH;
//    frame_comunicacao.tipo_etiqueta = TIPO_ETIQUETA_UNIDADE_OU_KG;
//    frame_comunicacao.tamanho_dados = (unsigned char)sizeof(TEtiqueta_un_ou_kg);
//    memcpy(buffer_etiqueta, (unsigned char *)&etiqueta_un_ou_kg, sizeof(TEtiqueta_un_ou_kg));
//    crc_calculado = calcula_crc(buffer_etiqueta, sizeof(TEtiqueta_un_ou_kg));
//    frame_comunicacao.crc_low = (unsigned char)(crc_calculado & 0x00FF);
//    frame_comunicacao.crc_high = (unsigned char)((crc_calculado >> 8) & 0x00FF);
//
//    memcpy(buffer_envio, (unsigned char *)&frame_comunicacao, sizeof(TEtiqueta_un_ou_kg) - 2);
//    offset = sizeof(TEstrutura_comunicacao) - 2;
//    memcpy(buffer_envio + offset, (unsigned char *)&buffer_etiqueta, sizeof(TEtiqueta_un_ou_kg));
//    offset = offset + sizeof(TEtiqueta_un_ou_kg);
//    memcpy(buffer_envio + offset, (unsigned char *)&crc_calculado, sizeof(unsigned short));
//
//    /* Envia sinal para etiuqeta ter tempo de acordar */
//    Serial2.write(buffer_para_acordar_etiqueta, sizeof(buffer_para_acordar_etiqueta));
//    delay(2000);
//
//    /* Envia etiqueta para o dispositivo de etiqueta eletrônica */
//    tamanho_buffer_envio = sizeof(TEstrutura_comunicacao) + sizeof(TEtiqueta_un_ou_kg);
//    Serial2.write(buffer_envio, tamanho_buffer_envio);
//    Serial2.flush();
//    Serial.println("Etiqueta enviada");
//    delay(15000);
//
//    /* Popula etiqueta (tipo: oferta) e frame de comunicação */
//    Serial.println("* Enviando etiqueta do tipo oferta...");
//
//    memset((unsigned char *)&etiqueta_oferta, 0x00, sizeof(TEtiqueta_oferta));
//    sprintf(etiqueta_oferta.titulo_oferta, "OFERTA");
//    sprintf(etiqueta_oferta.nome_produto, "Choc importado");
//    sprintf(etiqueta_oferta.preco_produto, "R$20,99");
//    sprintf(etiqueta_oferta.complemento_produto, "1 un");
//    
//    frame_comunicacao.byte_1_sincronia = BYTE_1_SINCRONIA;
//    frame_comunicacao.byte_2_sincronia = BYTE_2_SINCRONIA;
//    frame_comunicacao.byte_3_sincronia = BYTE_3_SINCRONIA;
//    frame_comunicacao.byte_4_sincronia = BYTE_4_SINCRONIA;
//    frame_comunicacao.byte_5_sincronia = BYTE_5_SINCRONIA;
//    frame_comunicacao.byte_6_sincronia = BYTE_6_SINCRONIA;
//    frame_comunicacao.endereco_low = ENDERECO_ETIQUETA_LOW;
//    frame_comunicacao.endereco_high = ENDERECO_ETIQUETA_HIGH;
//    frame_comunicacao.tipo_etiqueta = TIPO_ETIQUETA_OFERTA;
//    frame_comunicacao.tamanho_dados = (unsigned char)sizeof(TEtiqueta_oferta);
//    memcpy(buffer_etiqueta, (unsigned char *)&etiqueta_oferta, sizeof(TEtiqueta_oferta));
//    crc_calculado = calcula_crc(buffer_etiqueta, sizeof(TEtiqueta_oferta));
//    frame_comunicacao.crc_low = (unsigned char)(crc_calculado & 0x00FF);
//    frame_comunicacao.crc_high = (unsigned char)((crc_calculado >> 8) & 0x00FF);
//
//    memcpy(buffer_envio, (unsigned char *)&frame_comunicacao, sizeof(TEstrutura_comunicacao) - 2);
//    offset = sizeof(TEstrutura_comunicacao) - 2;
//    memcpy(buffer_envio + offset, (unsigned char *)&buffer_etiqueta, sizeof(TEtiqueta_oferta));
//    offset = offset + sizeof(TEtiqueta_oferta);
//    memcpy(buffer_envio + offset, (unsigned char *)&crc_calculado, sizeof(unsigned short));
//
//    /* Envia sinal para etiuqeta ter tempo de acordar */
//    Serial2.write(buffer_para_acordar_etiqueta, sizeof(buffer_para_acordar_etiqueta));
//    delay(2000);
//
//    /* Envia etiqueta para o dispositivo de etiqueta eletrônica */
//    tamanho_buffer_envio = sizeof(TEstrutura_comunicacao) + sizeof(TEtiqueta_oferta);
//    Serial2.write(buffer_envio, tamanho_buffer_envio);
//    Serial2.flush();
//    Serial.println("Etiqueta enviada");
//    delay(15000);    
}
