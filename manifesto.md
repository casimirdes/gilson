
# SOBRE GILSON

#### PRINCÍPIO DE DESABAFO SOBRE:

Parta do princípio de quem trabalha com microcontroladores (desde um PIC16F84 até um ESP32-S3), sendo assim, temos espécies de vida bem diferentes no quesito memória RAM, memória de armazenamento tipo FLASH e/ou EEPROM, o poder de processamento também é muito baixo se comparado a um “Raspberry Pi” da vida ou o que dirá a um computador x86… Parta do princípio que tem uma galera do querido Arduino, que adora fazer “coisas de alto nível” querer rodas em um simples AVR ATMEGA328 com 2kB de RAM, 32kB de Flash e 16MHz de CPU. Por um lado isso é muito bom pois tem muita biblioteca de Arduino que eu bato palma e acho incrível como o pessoal faz para tal coisa funcionar em harmonia no Arduino UNO por exemplo, por outro lado, temos um “mundo” fechado com o Arduino e se você quiser pegar tal biblioteca e utilizar em outra plataforma pode ser uma tarefa difícil e é melhor procurar outra solução ou reinventar a roda… Dito isso nosso foco em questão somado a minha implicação pessoal é com o formato JSON que não passa de um “stringão” comedor de RAM, eis minha revolta! E o nome desse ‘formato’ que vem para quebrar o JSON é batizado como GILSON e caso já exista algo do tipo, to nem aiiiiiiii, viva a vida caseiramente.

#### OBJETIVO/FOCO: 
Me parece que JSON é o que domina no mundo da computação WEB para os diversos fins como troca de dados entre hierarquias iguais e/ou diferentes e também como armazenagem dos mesmos. Apesar de ser um formato muito simples, direto e fácil de criar, manipular, e entender em linguagens como JavaScript, Python, Dart, o ponto "fraco" dele é quando queremos enviar/armazenar esse formato/forma, dependendo do JSON ele se torna grande (em bytes) pois no fundo ele é puramente um “stringão” e isso impacta ainda mais quando estamos no mundo dos microcontroladores onde temos limitações de memória RAM e FLASH. Isso é um baita problema! O foco é como tornar isso menos custoso no quesito RAM e FLASH quando microcontroladores querem usar o JSON ou algo parecido e especialmente em linguagem C.

#### RESUMO: 
Essa biblioteca é um serializador e/ou empacotador e/ou organizador (chame como quiser) de estrutura de dados fortemente tipada, isto é, se temos uma estrutura específica formada por inteiros, flutuantes, listas, strings, matrizes ou um JSON da vida e queremos empacotar para enviar para outro dispositivo ou salvar na memória ou arquivo... essa biblioteca tem a função de codificar e decodificar em um vetor de bytes com objetivo simples de tentar obter o menor tamanho possível em cada pacote criado. Ela é baseada no JSON pois essa é a inspiração principal para criação da biblioteca, isto é, lógica de chave e valor.

#### FUNCIONAMENTO: 
Suponha um JSON que é formado por chave:valor, essa biblioteca segue esse princípio onde para cada 'chave' que será um número inteiro, teremos o ‘valor’ correspondente onde definimos qual o tipo de dado que iremos codificar ou decodificar. Para as funções de “encode” e “decode” sempre teremos uma de “init” para iniciar algo e “end” para finalizar algo. Como esse projeto é algo bem caseiro, está propenso a grandes mudanças e atualizações mas até o momento se pensa em 4 formatos básico:
* MODO COMPACTO:
    * É literalmente salvar somente os dados brutos tipados e ordenados conforme a chamada da função de ‘encode’ e na função de ‘decode’ obrigatoriamente segue a mesma ordem.
* MODO COMPLETO:
    * Tem a base do “MODO COMPACTO” mas salva mais informações com tamanho do pacote, crc para validação do pacote e o tipo de cada ‘valor’ armazenado.
* MODO COMPACTO KV:
    * É o “MODO COMPACTO” mas salva também o nome das chaves como se fosse o JSON da vida.
* MODO COMPLETO KV:
    * É o “MODO COMPLETO” mas salva também o nome das chaves como se fosse o JSON da vida.
 
#### LIMITES:
* Cada pacote GILSON pode ter um tamanho máximo de 65535 bytes.
* O limite de chaves é 256, isto é, de 0 a 255.
* O limite de cada string de entrada seja simples ou uma lista de strings, é de até 255 bytes e suporta o formato “UTF-8” de codificação.
* Nos modos “KV” o nome da chave tem o limite de 16 bytes e “só” aceita caracteres alfanuméricos codificados em “ASCII”. (Evite usar nomes com caracteres especiais ou locuragens da vida, porém é possível via parâmetros, aumentar esse limite)


#### TIPAGEM:
Seguindo os fundamentos do JSON onde temos os tipos básicos de dados, partimos de um exemplo:
```
{
    "texto" : "Hola que tal?",
    "numero" : 42,
    "numero_flutuante" : 1234.5678,
    "booleano": true,
    "nulo": null
}
```

Conclue-se que temos 3 palavras “reservadas” que são “true”, “false” e “null” e precisamos nos atentar sobre isso! No mais, teremos o tipo strings que são os textos e números que podem ser inteiros ou flutuantes.
* E qual a analogia com a linguagem C para criação dessa biblioteca?
    * textos = por exemplo: ```const char *string = ”Hola que tal?”;```
    * números = por exemplo: ```int valor = 42; ou float temp = 1234.5678;```
    * booleanos = por exemplo: ```bool flag = false;```

Um detalhe importante e faz toda a diferença na vida é que quando trabalhamos com números que são o “valor” da chave, seja no na forma simples de número ou uma lista ou matriz de números, esses números podem ser separados na linguagem C como:
* ```int8_t``` ou ```uint8_t``` que ocupam 1 bytes de memória
* ```int16_t``` ou ```uint16_t``` que ocupam 2 bytes de memória
* ```int32_t``` ou ```uint32_t``` que ocupam 4 bytes de memória
* ```int64_t``` ou ```uint64_t``` que ocupam 8 bytes de memória
* ```float``` que ocupa 4 bytes de memória
* ```double``` que ocupa 8 bytes de memória

Então a mágica se dá em saber qual o formato mínimo que podemos armazenar esse número ou números, a fim de economizar em bytes. Se tratando de strings, sejam elas como nome das chaves ou como valor, não tem muito o que fazer para economizar espaço e não vamos abordar termos como compressão de dados entre outros.


#### LIMITAÇÕES DE USO:
* ```true```, ```false``` e ```null``` são tratados com ‘uint8_t’ como 1, 0, 2 respectivamente
* ainda não é possível ter listas com múltiplos dados na lista, como por exemplo uma chave que seja:
    * ```{"dados":["uma string",1234,987.3423,true]}```
* ainda não temos esquema de JSON dentro de JSON que seria objeto dentro do objeto como:
    * ```{"dados":[{"vai":1,"venha":123.34},{"saco":32,"chato":[1,2,3,4,5]}]}```
	

#### FUTURO: 
É claro que todas as limitações podem ser vencidas em uma futura versão da lib mas avaliar e sempre comparar por exemplo com um JSON original se vale a pena ou não utilizar a lib, pois em alguns casos pode ser que o JSON seja melhor do que utilizar essa lib mesmo que no quesito tamanho em bytes.

#### DEFINIÇÕES DE HARDWARE:
Dependendo de onde vamos utilizar essa biblioteca, é possível via parâmetros (como “defines”) deixar a biblioteca mais “econômica” de modo geral.


#### LINGUAGENS:
O foco principal é o uso em C, mas temos a versão para Python e futuramente para JavaScript e Dart.
