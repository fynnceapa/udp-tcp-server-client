# Tema 2 PCOM

## Aplicatie client-server TCP si UDP pentru gestionarea mesajelor

Am pornit de la laboratorul 7 de TCP si multiplexare.

Numarul de clienti nu este limitat pentru ca folosesc un vector resizeable.

Am verificat pe cat posibil valorile intoarse de apelurile de sistem folosind macro-ul `DIE`.

Am dezactivat algoritmul lui Nagle (linia 109 in `subscriber.cpp` si 148 in `server.cpp`).

Am dezactivat buffering-ul pentru stdout in primele linii din main in ambele fisiere.

Deoarece in cerinta se specifica ca nu trebuie sa afisam alte lucruri decat cele mentionate, in momentul in care se primeste de la tastatura o comanda necunoscuta nu se intampla nimic.

### Protocolul

- Conform cerintei, am implementat propriul meu protocol pentru a trimite date de la server la clientii TCP.
- campul `opcode` ii spune clientului ce tip de mesaj a primit (-1 == exit, 0 == int, 1 == short_real, 2 == float si 3 == string).
- campul `opcode` este folosit si pentru ca serverul sa stie ce actiune face clientul (10 == disconnect, 11 == subscribe, 12 == unsubscribe).
- pe langa topic si content am mai adaugat si campuri pentru portul si ip-ul clientului UDP de la care s-a emis mesajul.
- campul `topic` are dimensiunea 51 in loc de 50 pentru a putea fi adaugat caracterul `\0`.
- campul `content` este de 1500 (as fi vrut sa il fac sa poata avea orice dimensiune, lucru care ar fi facut totul mai eficient, dar nu am reusit).

### Structura de client

- am retinut pentru fiecare client topicurile la care este abonat folosint un map.
- campul `is_online` ne spune daca clientul mai are o conexiune activa sau nu; in momentul deconectarii se seteaza campul pe 0, dar nu se sterg alte lucruri.
- in cazul unei reconectari serverul va sti la ce topicuri era abonat clientul cu id-ul respectiv pentru ca un resetez map-ul.


### Modul de inchidere server

Pentru a inchide serverul trebuie sa inchid in mod corect si toti clientii conectati la el, astfel ca in momentul in care se primeste la **stdin** mesajul "exit", se trimite la toti clientii online un mesaj cu **opcode == -1**. In momentul in care un astfel de mesaj este primit de catre client acesta da close socket-ului folosit si se inchide.
 
### Subscriber

Aici nu a trebuit sa fac mare lucru.
In principal am implementat logica pentru multiplexare, comenzile de la tastatura (vezi functia `case_stdin`) si am aplicat formulele date in enunt pentru interpretarea diferitelor tipuri de date.
