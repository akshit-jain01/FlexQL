# FlexQL: In-Memory SQL Database Engine 

FlexQL is a lightweight, high-performance in-memory SQL database engine built from scratch in C++. It supports core relational operations including SELECT, INSERT, DELETE, JOIN, and ORDER BY using a columnar storage model.

---

##  Achievements Summary

- Peak Throughput: ~65,000–70,000 rows/sec (1M row benchmark)
- Functional Pass Rate: 22/22 Unit Tests Passed
- Architecture: Columnar storage + hash indexing + hash join
- Execution Model: Multi-threaded TCP server

---

##  Benchmark Results

| Scale | Task | Result | Throughput |
|------|------|--------|------------|
| 1M Rows | Bulk Insert | PASS | ~65,000 rows/sec |
| SELECT Scan | Full table scan | PASS | ~100 ms |
| Indexed Lookup | WHERE ID = x | PASS | ~microseconds |
| Join Query | Hash Join | PASS | < 1 ms |
| Range Query | BALANCE > x | PASS | ~milliseconds |
| Unit Tests | SQL correctness | PASS | 22/22 |

---

## System Architecture

### 1. Storage Engine (Columnar)

- Numeric columns: `vector<vector<double>>`
- String columns: `vector<vector<string>>`
- Column-wise storage improves cache locality and scan speed
- Efficient for analytical queries

---

### 2. Indexing Layer

- Primary Index:  
  `unordered_map<double, int>`

- Enables:
  - O(1) lookup for primary key queries

---

### 3. Query Execution Engine

- Projection pushdown (only required columns processed)
- WHERE optimization:
  - Fast path for primary key (O(1))
  - Scan-based fallback for range queries
- Hash Join:
  - Builds hash table on one relation
  - Probes using the other relation

---

### 4. ORDER BY

- Sorting based on row indices
- Supports numeric and string columns

---

### 5. Networking Layer

- TCP-based client-server architecture
- Multi-threaded request handling
- Buffered query parsing

---

### 6. Query Processing Pipeline

1. Client sends SQL query  
2. Server buffers until `;`  
3. Parser converts query → structured object  
4. Executor processes:
   - SELECT / INSERT / DELETE  
   - WHERE filtering  
   - JOIN (hash join)  
   - ORDER BY (sorting)  
5. Result returned to client  

---

##  Key Design Decisions

- Columnar storage → better cache locality  
- Hash index → O(1) lookup  
- Hash join → efficient joins  
- In-memory design → high throughput  

---

##  Limitations

- No persistence (in-memory only)  
- Limited SQL grammar  
- No transaction support  
- No secondary indexes (range queries use scan)  

---

##  Future Work

- Secondary indexing for range queries  
- B+ tree implementation  
- Query optimizer  
- GROUP BY / aggregation  
- Parallel query execution  

---

##  How to Run

### Build
```bash
make clean                     - cleans all previous build object files
make                           - builds all object files

```
### Run
```bash
make run (in terminal 1)- runs server on port 9000

make run_client (in terminal 2) - runs flexql client
```
### Run benchmark
```bash
g++ -Iinclude benchmark_flexql.cpp src/flexql/flexql.cpp -o bench2

./bench2
```
---
## Example Queries
```
SELECT NAME FROM USERS WHERE BALANCE > 1000;

SELECT NAME, BALANCE
FROM USERS
ORDER BY BALANCE DESC;

SELECT U.NAME, O.AMOUNT
FROM USERS U
JOIN ORDERS O
ON U.ID = O.USER_ID;
```
---
## Summary

FlexQL demonstrates core database system concepts including:

- Query parsing and execution
- Columnar storage design
- Hash-based indexing
- Join optimization
- Multi-threaded server design