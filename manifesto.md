OBJETIVO/FOCO:
JSON é o que domina no mundo da computacao WEB para os diversos fins
apesar de ser um formato muito simples, direto, fácil de criar/manipular/entender
o ponto "fraco" dele é quando queremos enviar/armazenar ele, dependendo do JSON
ele se torna grande (em bytes) e ainda mais quando estamos no mundo dos microcontroladores
onde temos limitaçoes de memória RAM e FLASH isso é um problema

---

RESUMO:
essa lib é um serializador/empacotador/organizador de estrutua de dados fortemente tipado
isto é, se temos uma estrutura específica formada por inteiros, flutuantes, listas, strings, matrizes
ou um JSON da vida ou uma lista, matriz... e queremos empacotar para enviar para outro dispositivo ou
salvar na memória ou arquivo... essa lib tem funcao de codificar e decodificar com objetivo simples de
tentar obter o menor tamanho possível em bytes de cada pacote criado
é claro que se for um JSON nos temos que saber exatamente cada tipo de dado de nosso JSON

---

FUNCIONAMENTO:
suponha como um JSON que é formado por chave:valor, aqui é algo parecido onde para cada 'chave' que será
um inteiro de 0 a 255, teremos o valor correspondente onde teremos que dizer qual o tipo de dado que iremos
codificar ou decodificar e temos 2 formatos de encode/decode que sao:
MODO COMPACTO:
que é literalente salvar somente os dados brutos tipados e ordenados conforme a chamada da
funcao de encode e na funcao de decode obrigatóriamente segue a mesma ordem
MODO COMPLETO:
atem de ter o objetivo do 'MODO COMPACTO', aqui é salvo o tipo de dado de cada 'chave' de entrada
salva quantas chaves temos no pacote e calcula um crc de validação do pacote, sendo assim na parte
de decodificar o pacote nao é necessário passar informaçoes sobre o tipo de dados e nem estar na mesma
ordenacao de codificação

---

LIMITES:
como é pensado em utilização em microcontroladores, assumimos um limite de até 65535 bytes (64kB)
sendo esse o limite de RAM que o pacote pode gerar tanto para encode como para decode
"limite de ram que é o vetor principal 'buf_geral[40k]'"
pensando na analogia de chave:valor, temos um limite de até 256 chaves (0 a 255)
e quando formos utilizar com matrizes, essas são no máximo 2D ou seja matriz[x][y]
e nao é possivel salvar matrizes com multiplos tipo de dados
para strings, o limite é até 255 bytes indiferente da codificação 'ascii' ou 'utf-8' por exemplo...

---

FUTURO:
é claro que todas as limitacoes podem ser vencidas em uma futura versao da lib
mas avaliar e sempre comparar por exemplo com um JSON original se vale a pena
ou nao utilizar a lib, pois em alguns casos pode ser que o JSON seja melhor
do que utilizar essa lib no quesito tamanho em bytes

---

EXPLICAÇÃO INICIAL:
HEADER MODO COMPACTO: offset de 1 byte
1B = MODO

HEADER MODO COMPLETO: offset de 8 bytes
1B = MODO
4B = crc pacote (crc32)
2B = tamanho pacote (até 65355 bytes)
1B = até 256 chaves (nao soma as listas, trata como 1)

defines tipos básicos:
0 = bit				depende de quantos teremos mas pode ser alocados em 1 a 4 bytes todos juntos (nada feito ainda...)
1 = int8			1 bytes
2 = uint8			1 bytes
3 = int16			2 bytes
4 = uint16			2 bytes
5 = int32			4 bytes
6 = uint32			4 bytes
7 = int64			8 bytes
8 = uint64			8 bytes
9 = float32			4 bytes
10 = float64		8 bytes
11 = string			dinâmico, 1 até 255 bytes, seguido de 1 byte com o tamanho, codificação utf-8 padrão!!! tamanho é bruto
12 a 63 = reservado para novos

