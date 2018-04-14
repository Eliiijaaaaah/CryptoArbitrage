#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>

#define ExchangeCount 6
#define CoinCount 6

/* See https://curl.haxx.se/libcurl/c/getinmemory.html */
struct MemoryStruct {
  char *memory;
  size_t size;
};

/* tracks a specific Coin's data */
struct Coin{
  char Name[4];
  char exchange[30];
  double price;
  struct Coin *next;
};

/* the list of Coins */
struct List{
  char minExchange[30];
  char maxExchange[30];
  double minPrice;
  double maxPrice;
  double diff;
  struct Coin *head;
};


/* Function headers -- all functions below main */
/* JSON functions */
double ParseJSON(char *json);
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);

/* Coin structure functions */
struct Coin *CreateCoin(char coinName[], double price, char exchangeName[]);
struct List *CreateList();
void addCoin(struct List *list, char coinName[], double price, char exchangeName[]);

/* Main Functinós */
double getPrice(char *specificExchange, char *specificCoin);
void setMin(struct List *list, struct Coin *coin);
void setMax(struct List *list, struct Coin *coin);

int main(){
  double price, diff = 0;
  int i, j, best;
  char coinName[4];


  char coin[CoinCount][4] = {
    { "BTC" },
    { "ETH" },
    { "LTC" },
    { "ZEC" },
    { "XRP" },
    { "XMR" }
  };

  char exchange[ExchangeCount][30] = {
    { "Coinbase" },
    { "CCCAGG" },
    { "Kraken" },
    { "Bitfinex" },
    { "Bitstamp" },
    { "Gemini" }
  };

  char exchangeFees[ExchangeCount];

  struct List *theList[CoinCount];

  /* Create list of coins */
  for (i = 0; i < CoinCount; i++){

    /* Creating separate lists for each coin */
    theList[i] = CreateList();

    /* Create list of exchanges */
    for (j = 0; j < ExchangeCount; j++){
      price = getPrice(&exchange[j][0], &coin[i][0]);

      if (price != 0){
        printf("%s price on %s is %lf\n", &coin[i][0], &exchange[j][0], price);

        addCoin(theList[i], &coin[i][0], price, &exchange[j][0]);
      }
      else{
        printf("%s price on %s is %s\n", &coin[i][0], &exchange[j][0], "UNAVAILABLE");
      }
    }

    strcpy(coinName, &coin[i][0]);
    printf("\n");
    j = 0;
  }

  /* Set mins and maxes */
  for (i = 0; i < CoinCount; i++){
    setMin(theList[i], theList[i]->head);
    setMax(theList[i], theList[i]->head);
  }

  /* Find the greatest difference */
  for (i = 0; i < CoinCount; i++){
    theList[i]->diff = (1 - theList[i]->minPrice / theList[i]->maxPrice);

    if (theList[i]->diff > diff){
      diff = theList[i]->diff;
      best = i;
    }
  }

  printf("Buy %s on %s, then sell on %s for a %lf percent gain.\n", &coin[best][0], theList[best]->minExchange, theList[best]->maxExchange, diff*100);

  return 0;
}

void setMin(struct List *list, struct Coin *coin){
  if (coin == NULL){
    return;
  }

  if (list->minPrice == 0){
    strcpy(list->minExchange, coin->exchange);
    list->minPrice = coin->price;
  }

  if (list->minPrice > coin->price){
    strcpy(list->minExchange, coin->exchange);
    list->minPrice = coin->price;
  }

  setMin(list, coin->next);
}

void setMax(struct List *list, struct Coin *coin){
  if (coin == NULL){
    return;
  }

  if (list->maxPrice < coin->price){
    strcpy(list->maxExchange, coin->exchange);
    list->maxPrice = coin->price;
  }

  setMax(list, coin->next);
}

double getPrice(char *specificExchange, char *specificCoin)
{
  CURL *curl;
  CURLcode res;

  struct MemoryStruct chunk;
  chunk.memory = malloc(1); /* will grow as needed */
  chunk.size = 0;

  char url[100];

  double price = 0;

  curl = curl_easy_init();

  /* init the curl session */
  curl = curl_easy_init();

  /* specify URL to get */
  strcpy(url, "https://min-api.cryptocompare.com/data/price?fsym=");
  strcat(url, specificCoin);
  strcat(url, "&tsyms=USD&e=");
  strcat(url, specificExchange);

  curl_easy_setopt(curl, CURLOPT_URL, url);

  /* send all data to this function  */
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

  /* we pass our 'chunk' struct to the callback function */
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

  /* get it! */
  res = curl_easy_perform(curl);

  /* check for errors */
  if(res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
    curl_easy_strerror(res));
  }
  else{
    price = ParseJSON(chunk.memory);
  }

  /* always cleanup */
  curl_easy_cleanup(curl);

  return price;
}

/* see https://json-c.github.io/json-c/json-c-0.10/doc/html/json__tokener_8h.html#abf031fdf1e5caab71e2225a99588c6bb and https://linuxprograms.wordpress.com/2010/08/19/json_object_new_object/ */
double ParseJSON(char *json){
  double result;
  json_object * jobj = json_tokener_parse(json);

  json_object_object_foreach(jobj, key, val) { /*Passing through every array element*/
    jobj = json_object_object_get(jobj, key);
  }

  result = json_object_get_double(jobj);
  return result;
}

/* See https://curl.haxx.se/libcurl/c/getinmemory.html */
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

struct Coin *CreateCoin(char coinName[], double price, char exchangeName[]){
  /* Allocate memory for new node */
  struct Coin *newCoin = (struct Coin*) malloc(sizeof(struct Coin));

  if (newCoin == NULL){
    printf("Not enough memory left on system\n");
    return newCoin;
  }
  else{
    strcpy(newCoin->Name, coinName);
    newCoin->price = price;
    strcpy(newCoin->exchange, exchangeName);
    newCoin->next = NULL;

    return newCoin;
  }
}

struct List *CreateList(){
  /* Allocate memory for new list */
  struct List *list = (struct List*) malloc(sizeof(struct List));

  if (NULL==list){
      printf("Not enough memory left on the system\n");
      return list;
  }
  else{
    memset(list->minExchange, 0, sizeof(list->minExchange));;
    memset(list->maxExchange, 0, sizeof(list->maxExchange));;
    list->minPrice = 0;
    list->maxPrice = 0;
    list->diff = 0;
    list->head = NULL;
    return list;
  }
}

void addCoin(struct List *list, char coinName[], double price, char exchangeName[]){
  struct Coin *newCoin = CreateCoin(coinName, price, exchangeName);
  struct Coin *temp;

  if (list->head != NULL){
    temp = list->head;
    newCoin->next = temp;
  }

  list->head = newCoin;
  return;
}
