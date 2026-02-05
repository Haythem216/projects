//Navbar code
function openNav() {
    document.getElementById("mySidenav").style.width = "250px";
  }
  
function closeNav() {
    document.getElementById("mySidenav").style.width = "0";
  }






// Function to calculate the total cost
function calculateCost(city, hotelQuality, transportation, nights, numPeople) {
    let totalCost = 0;

    // Calculate the accommodation cost
    if (city == "Hammamet") {
        totalCost += nights * ((hotelQuality == "3-star") ? 30 : (hotelQuality == "4-star") ? 70 : 100);
    } else if (city == "Mahdia") {
        totalCost += nights * ((hotelQuality == "3-star") ? 25 : (hotelQuality == "4-star") ? 60 : 80);
    } else if (city == "Tozeur") {
        totalCost += nights * ((hotelQuality == "3-star") ? 25 : (hotelQuality == "4-star") ? 60 : 100);
    }

    // Calculate the transportation cost
    if (transportation == "PublicTransportation") {
        totalCost += nights * 5;
    } else if (transportation == "CarRental") {
        totalCost += nights * 50;
    } else if (transportation == "PrivateVehicle") {
        totalCost += 0; // Assuming no cost for private vehicle
    }

    // Calculate the total cost for the number of people
    totalCost *= numPeople;

    return totalCost;
}

// Get the modal
var modal = document.getElementById("myModal");

// Get the <span> element that closes the modal
var span = document.getElementsByClassName("close")[0];

// When the user clicks on the button, open the modal
document.getElementById("myForm").addEventListener("submit", function (event) {
    event.preventDefault();
    modal.style.display = "block";

    // Get form data
    var name = document.getElementById('username').value;
    var city = document.getElementById('city').value;
    var hotelQuality = document.getElementById('HotelQuality').value;
    var transportation = document.getElementById('transportation').value;
    var nights = parseInt(document.getElementById('Nights').value);
    var numPeople = parseInt(document.getElementById('NbPeople').value);

    // Calculate the cost
    var totalCost = calculateCost(city, hotelQuality, transportation, nights, numPeople);

    // Display the result in the modal
    document.getElementById('modalContent').innerText = "Hi " + name + "! You chose to stay in " + city + ' in a ' + hotelQuality + ' Hotel, for ' + nights + ' nights, and you chose to transport via ' + transportation + '. The total cost for your trip, for '+numPeople+' people, is approximately: $' + totalCost;
});

// When the user clicks on <span> (x), close the modal
span.onclick = function () {
    modal.style.display = "none";
}

// When the user clicks anywhere outside of the modal, close it
window.onclick = function (event) {
    if (event.target == modal) {
        modal.style.display = "none";
    }
}



